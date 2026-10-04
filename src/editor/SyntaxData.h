#pragma once

#include <qce/kate/KateSyntaxIndex.h>

// Application-wide view of the Kate syntax data managed by qcodeedit
// (qce::kate::dataDir()). Shared by all editor tabs and viewers.
namespace SyntaxData {

/// Index over qce::kate::syntaxDir(); loaded on first use.
const qce::kate::KateSyntaxIndex& index();

/// Re-read index.json and the syntax directory (e.g. after a download).
void reload();

} // namespace SyntaxData
