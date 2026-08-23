#include "HodionTsf.h"

// CLSID của COM server (đồng thời là ID text service đăng ký với TSF).
// {3FBE1B8E-9C52-4B7A-8E1D-5A642F0C917B}
const CLSID CLSID_HodionKeyService = {
    0x3fbe1b8e, 0x9c52, 0x4b7a, {0x8e, 0x1d, 0x5a, 0x64, 0x2f, 0x0c, 0x91, 0x7b}};

// Language profile tiếng Việt của HodionKey.
// {A1E6F0D3-27C4-45B9-9B02-8F33D16E4A25}
const GUID GUID_HodionKeyProfile = {
    0xa1e6f0d3, 0x27c4, 0x45b9, {0x9b, 0x02, 0x8f, 0x33, 0xd1, 0x6e, 0x4a, 0x25}};

// Display attribute cho đoạn văn bản đang ghép (gạch chân).
// {5C8D94A7-1E3F-4D62-B077-C94E288A63F1}
const GUID GUID_HodionKeyDisplayAttributeInput = {
    0x5c8d94a7, 0x1e3f, 0x4d62, {0xb0, 0x77, 0xc9, 0x4e, 0x28, 0x8a, 0x63, 0xf1}};
