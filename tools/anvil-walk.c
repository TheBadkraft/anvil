/* ********************************************************************** *
 * anvil-walk - proves that no document which loads clean can send a       *
 * walker round in circles (BR-2611-anvl-resolver-002).                    *
 *                                                                         *
 *   anvil-walk < list-of-files                                            *
 *                                                                         *
 * Reads one path per line from standard input, loads each with            *
 * anvil_load, and, for a document that loads without errors, walks it      *
 * through the public accessors only (anvil_flat.h): every top-level        *
 * statement's value, then, depth first, every array or tuple element and  *
 * every object field. Those accessors are transparent through a resolved  *
 * `$` reference, so a reference that led back to a container holding it   *
 * would make this walk endless. The walk therefore notes the values it is  *
 * inside, and carries a depth and a visit cap: coming back to a value it   *
 * is inside, or reaching a cap, is a failure ("loop"). The included        *
 * documents are walked too, through anvil_document_get_includes.          *
 *                                                                         *
 * anvil-dump does not follow `resolved`, which is why the corpus and the  *
 * fuzzer never saw such a loop: this tool is the corpus's check for it.   *
 *                                                                         *
 * Output: one line per failure, then a summary. Exit status: 0 when no     *
 * document loops, 1 when one does, 2 for a usage error.                   *
 * ********************************************************************** */

#include "anvil_flat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WALK_DEPTH_CAP 4000      // a legitimate corpus document nests a few hundred levels at most
#define WALK_VISIT_CAP 5000000   // values entered in one document; a safeguard, the colours below bound a walk already
#define INCLUDE_DEPTH_CAP 64

/* A value handle is "grey" while the walk is inside it and "black" once it is done. Reaching a grey handle again
 * means the walk is inside a value it has come back to: a loop. A black handle is not walked twice, so a document
 * whose references fan out (a_k := [ $a_k-1, $a_k-1 ], forty times, is 2^40 paths) costs as many steps as it has
 * handles. The depth cap catches a loop even if the handles were not stable. */
typedef struct {
   const void **key;
   unsigned char *state; // 1 grey, 2 black
   size_t cap, count;
} handle_set;

static size_t slot_of(const void *p, size_t cap) {
   size_t h = (size_t)(((unsigned long long)(size_t)p >> 4) * 0x9E3779B97F4A7C15ull >> 20);
   return h & (cap - 1);
}

static bool set_grow(handle_set *s) {
   size_t cap = s->cap ? s->cap * 2 : 1024;
   const void **key = calloc(cap, sizeof *key);
   unsigned char *state = calloc(cap, 1);
   if (!key || !state) {
      free(key);
      free(state);
      return false;
   }
   for (size_t i = 0; i < s->cap; i++) {
      if (s->key[i]) {
         size_t at = slot_of(s->key[i], cap);
         while (key[at]) {
            at = (at + 1) & (cap - 1);
         }
         key[at] = s->key[i];
         state[at] = s->state[i];
      }
   }
   free(s->key);
   free(s->state);
   s->key = key;
   s->state = state;
   s->cap = cap;
   return true;
}

// The slot of p, inserting it (state 0) when absent; NULL when out of memory.
static unsigned char *set_slot(handle_set *s, const void *p) {
   if (s->count * 2 >= s->cap && !set_grow(s)) {
      return NULL;
   }
   size_t at = slot_of(p, s->cap);
   while (s->key[at]) {
      if (s->key[at] == p) {
         return &s->state[at];
      }
      at = (at + 1) & (s->cap - 1);
   }
   s->key[at] = p;
   s->count++;
   return &s->state[at];
}

typedef struct {
   size_t visits;
   bool capped;
   handle_set seen;
} walk_state;

static void walk_value(anvil_value v, size_t depth, walk_state *w) {
   if (!v || w->capped) {
      return;
   }
   if (depth > WALK_DEPTH_CAP || ++w->visits > WALK_VISIT_CAP) {
      w->capped = true;
      return;
   }
   unsigned char *st = set_slot(&w->seen, v);
   if (!st) {
      w->capped = true;
      return;
   }
   if (*st == 1) {
      w->capped = true; // back inside a value the walk is in: a loop
      return;
   }
   if (*st == 2) {
      return;
   }
   *st = 1;
   anvil_value_type t = anvil_value_get_type(v);
   if (t == ANVIL_VALUE_ARRAY || t == ANVIL_VALUE_TUPLE) {
      size_t n = anvil_value_get_count(v);
      for (size_t i = 0; i < n && !w->capped; i++) {
         walk_value(anvil_value_get_element(v, i), depth + 1, w);
      }
   } else if (t == ANVIL_VALUE_OBJECT) {
      size_t n = anvil_value_get_count(v);
      for (size_t i = 0; i < n && !w->capped; i++) {
         walk_value(anvil_statement_get_value(anvil_value_get_statement(v, i)), depth + 1, w);
      }
   }
   // the slot may have moved if the set grew inside the recursion: look it up again
   st = set_slot(&w->seen, v);
   if (st) {
      *st = 2;
   }
}

static void walk_document(anvil_document doc, size_t include_depth, walk_state *w) {
   anvil_statement_iterator it = anvil_document_get_statements(doc);
   anvil_statement st = NULL;
   while (it && !w->capped && anvil_statement_iterator_next(it, &st)) {
      walk_value(anvil_statement_get_value(st), 0, w);
   }
   anvil_statement_iterator_dispose(it);
   if (include_depth >= INCLUDE_DEPTH_CAP) {
      w->capped = true;
      return;
   }
   anvil_document_iterator inc = anvil_document_get_includes(doc);
   anvil_document child = NULL;
   while (inc && !w->capped && anvil_document_iterator_next(inc, &child)) {
      walk_document(child, include_depth + 1, w);
      anvil_dispose(child);
   }
   anvil_document_iterator_dispose(inc);
}

int main(int argc, char **argv) {
   (void)argv;
   if (argc != 1) {
      fprintf(stderr, "usage: anvil-walk < list-of-files\n");
      return 2;
   }
   char path[4096];
   size_t files = 0, walked = 0, rejected = 0, loops = 0, visits = 0;
   while (fgets(path, sizeof path, stdin)) {
      size_t len = strlen(path);
      if (len && path[len - 1] == '\n') {
         path[--len] = '\0';
      }
      if (len == 0) {
         continue;
      }
      files++;
      anvil_document doc = anvil_load(path);
      if (!doc || anvil_has_errors(doc)) {
         rejected++; // the loader refused it: there is no resolved document to walk
         anvil_dispose(doc);
         continue;
      }
      walk_state w = {0};
      walk_document(doc, 0, &w);
      free(w.seen.key);
      free(w.seen.state);
      walked++;
      visits += w.visits;
      if (w.capped) {
         loops++;
         printf("LOOP %s (the walk came back inside a value it was in, or reached its cap, after %zu values)\n", path, w.visits);
      }
      anvil_dispose(doc);
   }
   printf("anvil-walk: %zu files, %zu loaded clean and walked (%zu values), %zu refused by the loader, %zu loops\n", files, walked,
          visits, rejected, loops);
   return loops ? 1 : 0;
}
