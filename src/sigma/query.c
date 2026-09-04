/*
 * Sigma Collections
 * Copyright (c) 2026 David Boarman (BadKraft) and contributors
 * QuantumOverride [Q|]
 * ----------------------------------------------------------------------- *
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
 * ----------------------------------------------------------------------- *
 * File: query.c
 * Description: Source file for Sigma Collections' heapless query mechanism
 */

#include <sigma/query.h>

static bool query_next(sc_queryable *q, const void **out_element, usize *out_index) {
   if (!q || !q->advance) {
      return false;
   }
   const void *elem = NULL;
   usize idx = 0;
   if (!q->advance(q, &elem, &idx)) {
      return false;
   }
   if (out_element) {
      *out_element = elem;
   }
   if (out_index) {
      *out_index = idx;
   }
   return true;
}

static bool query_first(sc_queryable q, sc_predicate_fn pred, void *userdata, usize *out_index) {
   if (!pred) {
      return false;
   }
   const void *elem;
   usize idx;
   while (query_next(&q, &elem, &idx)) {
      if (pred(elem, idx, userdata)) {
         if (out_index) {
            *out_index = idx;
         }
         return true;
      }
   }
   return false;
}

const sc_query_i Query = {
   .next = query_next,
   .first = query_first,
};
