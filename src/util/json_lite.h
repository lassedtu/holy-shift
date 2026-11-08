#ifndef UTIL_JSON_LITE_H
#define UTIL_JSON_LITE_H

#include <stdbool.h>

// Small reader for the JSON files of the game
// A key must be unique in the file, the reader finds the first match

// Return the text after a key and its colon, or NULL if the key is missing
const char *JsonFindKey(const char *text, const char *key);

// Read a number that follows a key
bool JsonReadNumber(const char *text, const char *key, double *value);

// Read a string that follows a key into text, return false when the key or the string is missing
// The string must not have escaped quotes, a string that is too long is cut
bool JsonReadString(const char *text, const char *key, char *value, int size);

// Read true or false that follows a key
bool JsonReadBool(const char *text, const char *key, bool *value);

// Read an array of numbers that follows a key, return the count or -1 on error
int JsonReadNumberArray(const char *text, const char *key, float *values, int max_count);

// Look in an array of strings that follows a key for a string
// Return false when the key or the array is missing, found tells if the string is in the array
bool JsonArrayHasString(const char *text, const char *key, const char *value, bool *found);

// Skip spaces and the characters in skip, then return the next character
const char *JsonSkip(const char *position, const char *skip);

#endif
