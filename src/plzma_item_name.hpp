//
// xzip: the name a 7z item stored without one is extracted under.
//

#ifndef __PLZMA_ITEM_NAME_HPP__
#define __PLZMA_ITEM_NAME_HPP__ 1

#include <cstdio>

#include "../libplzma.hpp"

#include "CPP/7zip/Archive/IArchive.h"
#include "CPP/Windows/PropVariant.h"

namespace plzma {

    // A 7z may store an item with no name (7-Zip gives one only when it knows the source file), and
    // upstream refused to extract such an item at all ("Can't read item path."). It gets the name
    // 7-Zip uses when it has no archive name to fall back on, "[Content]" — numbered by position
    // when the archive holds more than one item, so two nameless items cannot overwrite each other.
    // xz is excluded: its single item is unnamed by design and callers name it themselves. So is any
    // archive whose headers 7-Zip flagged as broken: there the name was not absent but unreadable, and
    // inventing one turns a damaged header into a "successful" extraction of an empty file.
    inline void setFallbackItemPath(IInArchive * archive, const UInt32 index, const plzma_file_type type,
                                    Path & path) {
        if (path.count() != 0 || type == plzma_file_type_xz || !archive) {
            return;
        }
        NWindows::NCOM::CPropVariant errorFlags;
        if (archive->GetArchiveProperty(kpidErrorFlags, &errorFlags) == S_OK && errorFlags.vt == VT_UI4 &&
            (errorFlags.ulVal & kpv_ErrorFlags_HeadersError) != 0) {
            return;
        }
        UInt32 count = 0;
        archive->GetNumberOfItems(&count);
        if (count <= 1) {
            path.set("[Content]");
        } else {
            char name[32];
            snprintf(name, sizeof(name), "[Content] %u", static_cast<unsigned>(index) + 1);
            path.set(name);
        }
    }

} // namespace plzma

#endif // !__PLZMA_ITEM_NAME_HPP__
