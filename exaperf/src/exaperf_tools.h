/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef EXAPERF_TOOLS_H
#define EXAPERF_TOOLS_H

/* Remove all whitespace at beginning and end of string,
 * including newline and carriage return (if any).
 * Replaces contiguous occurrences of spaces with a
 * single space
 */
void remove_whitespace(char *str);

/*
 * Get a pointer to the last character of a string, not including '\0'.
 */
const char *last_character(const char *str);

#endif /* EXAPERF_TOOLS_H */
