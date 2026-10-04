# bomi's bridge to subliminal (https://github.com/Diaoul/subliminal).
#
# Compiled into bomi as a Qt resource (rsclist.qrc), written to a temporary
# directory by SubliminalFinder and run with the system python3. Commands, each printing one JSON document on stdout:
#
#   check
#       If subliminal imports: its version, the configuration file and the
#       providers, each with the options it accepts, the ones the configuration
#       sets (values hidden except usernames) and whether bomi searches it.
#   search --cache FILE --languages en,fr [--video PATH | --name NAME]
#       A list of candidates, best first. The subtitle objects are pickled to
#       FILE so that download can fetch one without searching again.
#   download --cache FILE --id ID --out PATH
#       Writes the subtitle's bytes to PATH: {"format": "srt"}.
#
# Errors print {"error": "..."} and exit with status 1. Provider failures are
# logged by subliminal on stderr and the provider is skipped.
#
# Provider credentials come from subliminal's own configuration file, the one
# its command line reads (~/.config/subliminal/subliminal.toml, or
# $SUBLIMINAL_CONFIG); bomi never writes it. Each [provider.NAME] table is
# passed to that provider as is, e.g.
#
#   [provider.opensubtitlescom]
#   username = "..."
#   password = "..."

import argparse
import inspect
import json
import logging
import os
import pickle
import sys
from datetime import timedelta

# Providers searched by default.
PROVIDERS = ['addic7ed', 'bsplayer', 'gestdown', 'napiprojekt', 'opensubtitles',
             'podnapisi', 'subtis', 'subtitulamos', 'tvsubtitles']
# opensubtitlescom needs the user's own account to download, and the *vip
# variants need a paid one; they are searched only when configured.
ACCOUNT_PROVIDERS = ['opensubtitlescom', 'opensubtitlescomvip', 'opensubtitlesvip']
# Configuration values shown as they are; anything else is shown as "set".
SHOWN_VALUES = {'username'}
# Defaults under the user's configuration. opensubtitlescom pages through every
# result by default, but the API refuses pages past 20 and the provider then
# drops all results; 5 pages (about 250 subtitles) is plenty.
DEFAULT_CONFIGS = {'opensubtitlescom': {'max_result_pages': 5},
                   'opensubtitlescomvip': {'max_result_pages': 5}}

# What opensubtitles.org serves instead of a subtitle when it wants a VIP account.
ADVERT = b'osdb.link/vip'


def fail(message):
    print(json.dumps({'error': message}))
    sys.exit(1)


class FirstError(logging.Handler):
    """Remembers subliminal's first error, which names the cause: providers
    fail by logging, and later errors only report the consequences."""

    def __init__(self):
        super().__init__(logging.ERROR)
        self.message = ''

    def emit(self, record):
        if self.message:
            return
        e = record.exc_info[1] if record.exc_info else None
        if e is None:
            self.message = record.getMessage()
        else:
            name, text = type(e).__name__, str(e)
            self.message = name if text in ('', name) else '%s: %s' % (name, text)


first_error = FirstError()


def config_path():
    from platformdirs import PlatformDirs
    path = os.environ.get('SUBLIMINAL_CONFIG')
    if path:
        return os.path.expanduser(path)
    return os.fspath(PlatformDirs('subliminal').user_config_path / 'subliminal.toml')


def load_provider_configs():
    """The [provider.NAME] tables of subliminal's configuration, or {}."""
    import tomllib
    try:
        with open(config_path(), 'rb') as file:
            toml = tomllib.load(file)
    except FileNotFoundError:
        return {}
    except Exception as e:
        logging.warning('ignoring %s: %s', config_path(), e)
        return {}
    providers = toml.get('provider', {})
    if not isinstance(providers, dict):
        return {}
    return {name: table for name, table in providers.items() if isinstance(table, dict)}


def providers_to_use(configs):
    return PROVIDERS + [name for name in ACCOUNT_PROVIDERS if name in configs]


def with_defaults(configs):
    merged = {name: dict(table) for name, table in DEFAULT_CONFIGS.items()}
    for name, table in configs.items():
        merged.setdefault(name, {}).update(table)
    return merged


def configure_cache(region, version):
    # A file cache, so a login token outlives this process. bomi keeps its own,
    # one per subliminal version: an older version's entries may not read back.
    from platformdirs import PlatformDirs
    try:
        cache_dir = PlatformDirs('bomi').user_cache_path
        cache_dir.mkdir(parents=True, exist_ok=True)
        name = 'subliminal-%s.dbm' % version
        for old in cache_dir.glob('subliminal-*.dbm*'):
            if not old.name.startswith(name):
                old.unlink()
        region.configure('dogpile.cache.dbm', expiration_time=timedelta(days=30),
                         arguments={'filename': os.fspath(cache_dir / name)})
    except Exception as e:
        logging.warning('using a memory cache: %s', e)
        region.configure('dogpile.cache.memory', replace_existing_backend=True)


def provider_options(cls):
    options = []
    for name, param in inspect.signature(cls.__init__).parameters.items():
        if name in ('self', 'timeout') or param.kind in (param.VAR_POSITIONAL,
                                                         param.VAR_KEYWORD):
            continue
        options.append(name)
    return options


def check():
    import subliminal
    from importlib.metadata import entry_points

    configs = load_provider_configs()
    used = providers_to_use(configs)
    providers = []
    for entry in sorted(entry_points(group='subliminal.providers'), key=lambda e: e.name):
        info = {'name': entry.name, 'options': [], 'used': entry.name in used}
        try:
            info['options'] = provider_options(entry.load())
        except Exception as e:  # a broken plugin
            info['error'] = '%s: %s' % (type(e).__name__, e)
        info['configured'] = ['%s: %s' % (key, value if key in SHOWN_VALUES else 'set')
                              for key, value in configs.get(entry.name, {}).items()]
        providers.append(info)
    print(json.dumps({'version': subliminal.__version__, 'config': config_path(),
                      'configFound': os.path.isfile(config_path()),
                      'providers': providers}))


def language_code(language):
    try:
        code = language.alpha2
    except Exception:
        code = language.alpha3
    if language.country is not None:
        code += '-' + str(language.country)
    return code.lower()


def file_name(subtitle):
    name = str(subtitle.info or subtitle.id).strip().replace('/', '_')
    ext = (subtitle.subtitle_format or 'srt').lower()
    if not name.lower().endswith(('.srt', '.ass', '.ssa', '.sub', '.vtt', '.smi')):
        name += '.' + ext
    return name


def search(args):
    from babelfish import Language
    from subliminal import Video, list_subtitles, scan_video
    from subliminal.score import compute_score

    languages = set()
    for code in args.languages.split(','):
        code = code.strip()
        if not code:
            continue
        try:
            languages.add(Language.fromietf(code))
        except Exception:
            logging.warning('skipping unknown language %r', code)
    if not languages:
        fail('No valid language to search for')

    if args.video:
        try:
            video = scan_video(args.video)  # hashes help hash-matching providers
        except Exception:
            video = Video.fromname(args.video)
    elif args.name:
        video = Video.fromname(args.name)
    else:
        fail('Nothing to search for')

    configs = load_provider_configs()
    subtitles = list_subtitles({video}, languages, providers=providers_to_use(configs),
                               provider_configs=with_defaults(configs)).get(video, [])
    found = []
    for subtitle in subtitles:
        try:
            score = compute_score(subtitle, video)
        except Exception:
            score = 0
        found.append((score, subtitle))
    found.sort(key=lambda pair: pair[0], reverse=True)

    cache = {}
    result = []
    for score, subtitle in found:
        key = '%s:%s' % (subtitle.provider_name, subtitle.id)
        if key in cache:
            continue
        cache[key] = subtitle
        result.append({'id': key,
                       'provider': subtitle.provider_name,
                       'language': language_code(subtitle.language),
                       'fileName': file_name(subtitle),
                       'hearingImpaired': bool(subtitle.hearing_impaired),
                       'score': score})
    with open(args.cache, 'wb') as file:
        pickle.dump(cache, file)
    print(json.dumps(result))


def download(args):
    from subliminal import download_subtitles

    with open(args.cache, 'rb') as file:
        cache = pickle.load(file)
    subtitle = cache.get(args.id)
    if subtitle is None:
        fail('Unknown subtitle %s; search again' % args.id)
    configs = load_provider_configs()
    download_subtitles([subtitle], providers=providers_to_use(configs),
                       provider_configs=with_defaults(configs))
    content = subtitle.content or b''
    if not content:
        if first_error.message:
            fail('%s failed: %s' % (subtitle.provider_name, first_error.message))
        fail('%s returned nothing' % subtitle.provider_name)
    if ADVERT in content[:500]:
        fail('%s returned an advert instead of a subtitle' % subtitle.provider_name)
    with open(args.out, 'wb') as file:
        file.write(content)
    print(json.dumps({'format': (subtitle.subtitle_format or 'srt').lower()}))


def main():
    logging.basicConfig(level=logging.WARNING, stream=sys.stderr)
    logging.getLogger('subliminal').addHandler(first_error)
    parser = argparse.ArgumentParser()
    commands = parser.add_subparsers(dest='command', required=True)
    commands.add_parser('check')
    s = commands.add_parser('search')
    s.add_argument('--cache', required=True)
    s.add_argument('--languages', required=True)
    s.add_argument('--video')
    s.add_argument('--name')
    d = commands.add_parser('download')
    d.add_argument('--cache', required=True)
    d.add_argument('--id', required=True)
    d.add_argument('--out', required=True)
    args = parser.parse_args()

    try:
        import subliminal
        from subliminal import region
    except Exception as e:  # missing, or broken by a Python upgrade
        fail('subliminal is not usable: %s' % e)
    configure_cache(region, subliminal.__version__)

    try:
        if args.command == 'check':
            check()
        elif args.command == 'search':
            search(args)
        else:
            download(args)
    except SystemExit:
        raise
    except Exception as e:
        fail('%s: %s' % (type(e).__name__, e))


if __name__ == '__main__':
    main()
