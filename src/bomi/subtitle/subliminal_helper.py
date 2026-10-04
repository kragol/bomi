# bomi's bridge to subliminal (https://github.com/Diaoul/subliminal).
#
# Compiled into bomi as a Qt resource (rsclist.qrc), written to a temporary
# directory by SubliminalFinder and run with the system python3. Commands, each printing one JSON document on stdout:
#
#   check
#       {"version": "2.6.0"} if subliminal imports.
#   search --cache FILE --languages en,fr [--video PATH | --name NAME]
#       A list of candidates, best first. The subtitle objects are pickled to
#       FILE so that download can fetch one without searching again.
#   download --cache FILE --id ID --out PATH
#       Writes the subtitle's bytes to PATH: {"format": "srt"}.
#
# Errors print {"error": "..."} and exit with status 1. Provider failures are
# logged by subliminal on stderr and the provider is skipped.

import argparse
import json
import logging
import pickle
import sys

# opensubtitlescom needs the user's own account to download, and the *vip
# variants need a paid one; without credentials their results cannot be fetched.
PROVIDERS = ['addic7ed', 'bsplayer', 'gestdown', 'napiprojekt', 'opensubtitles',
             'podnapisi', 'subtis', 'subtitulamos', 'tvsubtitles']

# What opensubtitles.org serves instead of a subtitle when it wants a VIP account.
ADVERT = b'osdb.link/vip'


def fail(message):
    print(json.dumps({'error': message}))
    sys.exit(1)


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

    subtitles = list_subtitles({video}, languages, providers=PROVIDERS).get(video, [])
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
    download_subtitles([subtitle], providers=PROVIDERS)
    content = subtitle.content or b''
    if not content:
        fail('%s returned nothing' % subtitle.provider_name)
    if ADVERT in content[:500]:
        fail('%s returned an advert instead of a subtitle' % subtitle.provider_name)
    with open(args.out, 'wb') as file:
        file.write(content)
    print(json.dumps({'format': (subtitle.subtitle_format or 'srt').lower()}))


def main():
    logging.basicConfig(level=logging.WARNING, stream=sys.stderr)
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
    region.configure('dogpile.cache.memory')

    try:
        if args.command == 'check':
            print(json.dumps({'version': subliminal.__version__}))
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
