/*
 * SigmaCore
 * Copyright (c) 2026 David Boarman (BadKraft) and contributors
 * QuantumOverride [Q|]
 * ----------------------------------------------
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * ----------------------------------------------
 * File: system_alloc.h
 * Description: Concrete controller struct bodies for the sigma.system.alloc
 *              provider (FR-2609-sigmem-001 / SR-2609-sigma-collections-001).
 *
 *              sigma.core/allocator.h forward-declares sc_ctrl_base_s and
 *              bump_allocator (struct sc_bump_ctrl_s *) opaquely so the
 *              sc_allocator_i interface stays provider-agnostic. Each
 *              concrete provider ships the matching full struct bodies in
 *              its own header — this is sigma.system.alloc's, the
 *              counterpart to sigma.memory/include/memory.h.
 *
 *              NOT BINARY-COMPATIBLE with sigma.memory's sc_ctrl_base_s /
 *              sc_bump_ctrl_s — different fields, different sizes (not even
 *              sc_ctrl_base_s matches). A consumer must include exactly the
 *              header matching whichever provider .o it links:
 *                sigma.memory.o      -> <sigma.memory/memory.h>
 *                sigma.system.alloc.o -> <system_alloc.h>  (this file)
 *              Never both, never the mismatched pairing — src/system_alloc.c
 *              casts through this layout, and a caller reading the wrong
 *              one will dereference a function pointer at the wrong offset.
 *
 *              Shape matches anvil/include/sigma/memory.h exactly (the
 *              reference implementation this package brings into the
 *              sigma.* ecosystem), so Anvil's existing arena->alloc(arena,
 *              size) call sites (src/core/module.c, src/core/source.c)
 *              need only repoint the #include, not their logic.
 */
#pragma once

// Repointed from the upstream "sigma.core/allocator.h" / "sigma.core/types.h" to Anvil's own
// pre-existing <sigma/allocator.h> (which already transitively includes <sigma/types.h>) --
// verified byte-for-byte identical content to sigma.core's real headers (only the include-path
// convention differs), so this avoids vendoring a second physical copy of sc_alloc_policy et al.
// that would collide (a full `enum {...}` body, unlike a repeated typedef, is not something C
// allows to redeclare even when identical -- confirmed directly by the compile error this fix
// replaces). See vendor/sigma.core/README.md.
#include <sigma/allocator.h>

// ── Common controller header — first member of every controller type ──────
struct sc_ctrl_base_s {
    sc_alloc_policy policy;
};

// ── Arena block — internal chain link ──────────────────────────────────────
typedef struct sc_arena_block_s {
    struct sc_arena_block_s *next;
    usize capacity;
    usize used;
    // data follows in-memory: malloc(sizeof(sc_arena_block_s) + capacity)
} sc_arena_block_s;

// ── Bump controller — accessed as arena->alloc(arena, size) ────────────────
struct sc_bump_ctrl_s {
    sc_ctrl_base_s base;  // MUST be first — cast compatibility with sc_ctrl_base_s*
    void *(*alloc)(struct sc_bump_ctrl_s *self, usize size);
    sc_arena_block_s *head;
    sc_arena_block_s *current;
    usize default_block;
};
