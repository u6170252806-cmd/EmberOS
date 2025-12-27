/*
 * EmberOS Vi-like Text Editor
 * Simple line-based text editor
 */

#ifndef EMBEROS_EDITOR_H
#define EMBEROS_EDITOR_H

#include "ramfs.h"

namespace editor {

// Editor limits
constexpr size_t MAX_LINES = 256;
constexpr size_t MAX_LINE_LEN = 256;

// Editor result
enum class Result {
    SAVED,
    QUIT,
    ERROR
};

/*
 * Open the editor with a file
 * Creates the file if it doesn't exist
 * Returns result when editor exits
 */
Result edit(const char* filename);

} // namespace editor

#endif // EMBEROS_EDITOR_H
