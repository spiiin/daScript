options indenting = 4

module utf8_utils shared public

require strings


def public utf8_encode(var to_array: array<uint8> &; ch: uint)
    if ch < 0x80
        to_array |> push(uint8(ch))
    elif ch < 0x800
        to_array |> push(uint8(0xC0 + (ch >> 6u)))
        to_array |> push(uint8(0x80 + (ch & 0x3F)))
    elif (ch < 0x10000)
        to_array |> push(uint8(0xE0 + (ch >> 12u)))
        to_array |> push(uint8(0x80 + ((ch >> 6u) & 0x3F)))
        to_array |> push(uint8(0x80 + (ch & 0x3F)))
    else
        to_array |> push(uint8(0xF0 + (ch >> 18u)))
        to_array |> push(uint8(0x80 + ((ch >> 12u) & 0x3F)))
        to_array |> push(uint8(0x80 + ((ch >> 6u) & 0x3F)))
        to_array |> push(uint8(0x80 + (ch & 0x3F)))


def public utf8_encode(ch: uint): array<uint8>
    if ch < 0x80
        return <- [{ uint8[] uint8(ch) }]
    elif ch < 0x800
        return <- [{ uint8[]
            uint8(0xC0 + (ch >> 6u));
            uint8(0x80 + (ch & 0x3F)) }]
    elif ch < 0x10000
        return <- [{ uint8[]
            uint8(0xE0 + (ch >> 12u));
            uint8(0x80 + ((ch >> 6u) & 0x3F));
            uint8(0x80 + (ch & 0x3F)) }]
    else
        return <- [{ uint8[]
            uint8(0xF0 + (ch >> 18u));
            uint8(0x80 + ((ch >> 12u) & 0x3F));
            uint8(0x80 + ((ch >> 6u) & 0x3F));
            uint8(0x80 + (ch & 0x3F)) }]


def public utf8_encode(var to_array: array<uint8> &; utf32_string: array<uint> & const)
    for ch in utf32_string
        to_array |> utf8_encode(ch)


def public utf8_encode(utf32_string: array<uint> & const): array<uint8>
    var to_array: array<uint8>
    to_array |> reserve(length(utf32_string))
    for ch in utf32_string
        to_array |> utf8_encode(ch)
    return <- to_array


def public utf8_length(utf8_string: array<uint8> & const): int
    var length = 0
    for ch in utf8_string
        if (uint(ch) & 0xC0) != 0x80
            length++
    return length


def public utf8_length(utf8_string: string const): int
    var length = 0
    for ch in utf8_string
        if (uint(ch) & 0xC0) != 0x80
            length++
    return length


def public is_first_byte_of_utf8_char(ch: uint8): bool
    let x = uint(ch)
    return (x > 0u) && (x < 128u || (x & 0xC0) == 0xC0)


def public contains_utf8_bom(utf8_string: array<uint8> & const): bool
    return (length(utf8_string) >= 3 &&
          uint(utf8_string[0]) == 0xEF && uint(utf8_string[1]) == 0xBB && uint(utf8_string[2]) == 0xBF)


def public contains_utf8_bom(utf8_string: string const): bool
    return (length(utf8_string) >= 3 &&
          uint(utf8_string |> character_at(0)) == 0xEF &&
          uint(utf8_string |> character_at(1)) == 0xBB &&
          uint(utf8_string |> character_at(2)) == 0xBF)



// Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

let s_utf8d <- [[ uint[]
    // The first part of the table maps bytes to character classes that
    // to reduce the size of the transition table and create bitmasks.
    0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;  0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;
    0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;  0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;
    0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;  0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;
    0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;  0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;0u;
    1u;1u;1u;1u;1u;1u;1u;1u;1u;1u;1u;1u;1u;1u;1u;1u;  9u;9u;9u;9u;9u;9u;9u;9u;9u;9u;9u;9u;9u;9u;9u;9u;
    7u;7u;7u;7u;7u;7u;7u;7u;7u;7u;7u;7u;7u;7u;7u;7u;  7u;7u;7u;7u;7u;7u;7u;7u;7u;7u;7u;7u;7u;7u;7u;7u;
    8u;8u;2u;2u;2u;2u;2u;2u;2u;2u;2u;2u;2u;2u;2u;2u;  2u;2u;2u;2u;2u;2u;2u;2u;2u;2u;2u;2u;2u;2u;2u;2u;
    10u;3u;3u;3u;3u;3u;3u;3u;3u;3u;3u;3u;3u;4u;3u;3u; 11u;6u;6u;6u;5u;8u;8u;8u;8u;8u;8u;8u;8u;8u;8u;8u;

    // The second part is a transition table that maps a combination
    // of a state of the automaton and a character class to a state.
     0u;12u;24u;36u;60u;96u;84u;12u;12u;12u;48u;72u; 12u;12u;12u;12u;12u;12u;12u;12u;12u;12u;12u;12u;
    12u; 0u;12u;12u;12u;12u;12u; 0u;12u; 0u;12u;12u; 12u;24u;12u;12u;12u;12u;12u;24u;12u;24u;12u;12u;
    12u;12u;12u;12u;12u;12u;12u;24u;12u;12u;12u;12u; 12u;24u;12u;12u;12u;12u;12u;12u;12u;24u;12u;12u;
    12u;12u;12u;12u;12u;12u;12u;36u;12u;36u;12u;12u; 12u;36u;12u;12u;12u;12u;12u;36u;12u;36u;12u;12u;
    12u;36u;12u;12u;12u;12u;12u;12u;12u;12u;12u;12u
]]


let UTF8_ACCEPT = 0u


def public is_utf8_string_valid(utf8_string: array<uint8> & const): bool
    var codepoint = 0u
    var state = 0u
    for ch in utf8_string
        let byte = uint(ch)
        let type_ = s_utf8d[byte]
        codepoint = (state != UTF8_ACCEPT) ? (byte & 0x3F) | (codepoint << 6u) : (0xFF >> type_) & byte
        state = s_utf8d[256u + state + type_]
    return state == UTF8_ACCEPT


def public is_utf8_string_valid(utf8_string: string const): bool
    var codepoint = 0u
    var state = 0u
    for ch in utf8_string
        let byte = uint(ch)
        let type_ = s_utf8d[byte]
        codepoint = (state != UTF8_ACCEPT) ? (byte & 0x3F) | (codepoint << 6u) : (0xFF >> type_) & byte
        state = s_utf8d[256u + state + type_]
    return state == UTF8_ACCEPT


def public utf8_decode(var utf32_string: array<uint> &; utf8_string: array<uint8> & const)
    var codepoint = 0u
    var state = 0u
    let length = length(utf8_string)
    var i = contains_utf8_bom(utf8_string) ? 3 : 0

    while i < length
        let byte = uint(utf8_string[i++])
        let type_ = s_utf8d[byte]
      	codepoint = (state != UTF8_ACCEPT) ? (byte & 0x3F) | (codepoint << 6u) : (0xFF >> type_) & byte
        state = s_utf8d[256u + state + type_]
        if state == UTF8_ACCEPT
            utf32_string |> push(codepoint)


def public utf8_decode(utf8_string: array<uint8> & const): array<uint>
    var utf32_string: array<uint>
    utf32_string |> utf8_decode(utf8_string)
    return <- utf32_string


def public utf8_decode(utf8_string: string const): array<uint>
    var utf32_string: array<uint>
    var arr <- [{ for c in utf8_string; uint8(c) }]
    utf32_string |> utf8_decode(arr)
    delete arr
    return <- utf32_string


def public utf8_decode(var utf32_string: array<uint> &; utf8_string: string const)
    var arr <- [{ for c in utf8_string; uint8(c) }]
    utf32_string |> utf8_decode(arr)
    delete arr

