#pragma once
/*
 * flat_internal.h - internal (hidden, never exported) access to what an anvil_document handle holds.
 * Used by tools/anvil-dump.c, so that the conformance dump reads exactly the structures the public
 * flat API reads, from the handle anvil_load* produced, instead of re-running the load pipeline.
 * Not part of the public ABI: not in libanvil.exports, and check-exports proves it stays hidden.
 */
#include "anvil_types.h"
#include "internal/module.h"

module_context anvil_internal_document_context(anvil_document doc);
module_document anvil_internal_document_root(anvil_document doc);
anvl_value anvil_internal_document_fragment(anvil_document doc);
