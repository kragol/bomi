#ifndef MPV_HELPER_HPP
#define MPV_HELPER_HPP

// This used to pull in mpv's internal options/m_option.h to build the option
// tables for bomi's af/vf filters. Those filters no longer exist -- modern mpv
// has no filter chain to inject them into -- so all that is left is the address
// marshalling used to pass pointers through mpv option strings.

template<class T>
SIA address_cast(const char *address, int base = 10)
-> typename std::enable_if<std::is_pointer<T>::value, T>::type
{
    bool ok = false;
    const quintptr ptr = QString::fromLatin1(address).toULongLong(&ok, base);
    return ok ? (T)(void*)(ptr) : (T)nullptr;
}

template<class T, class U>
SIA address_cast(U *ptr, int base = 10)
-> typename std::enable_if<std::is_same<T, QByteArray>::value
                           || std::is_same<T, QString>::value, T>::type
{
    return T::number((quint64)(quintptr)(void*)ptr, base);
}

#endif // MPV_HELPER_HPP
