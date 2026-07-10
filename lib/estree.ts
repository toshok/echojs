/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// The compiler's ESTree dialect.  Grows as the port proceeds; the goal
// is a faithful description of what the esprima fork produces plus the
// extensions our passes hang off the nodes.

export interface Position {
    line: number;
    column: number;
}

export interface SourceLocation {
    start: Position;
    end?: Position;
}
