#include "SyntaxData.h"

#include <qce/kate/KatePaths.h>

namespace SyntaxData {

static qce::kate::KateSyntaxIndex& storage()
{
    static qce::kate::KateSyntaxIndex idx = [] {
        auto i = qce::kate::KateSyntaxIndex::load(qce::kate::dataDir());
        i.saveIfDirty();
        return i;
    }();
    return idx;
}

const qce::kate::KateSyntaxIndex& index()
{
    return storage();
}

void reload()
{
    auto& idx = storage();
    idx = qce::kate::KateSyntaxIndex::load(qce::kate::dataDir());
    idx.saveIfDirty();
}

} // namespace SyntaxData
