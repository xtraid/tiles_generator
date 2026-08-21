#ifndef WANG_YANG_ZHANG_WITNESS_H
#define WANG_YANG_ZHANG_WITNESS_H

#include <stdbool.h>
#include <stddef.h>

#include "wang/solver.h"
#include "wang/tile.h"
#include "wang/yang_zhang.h"

/* Tri-state result for witness extraction and representation comparison. */
typedef enum {
    YANG_ZHANG_WITNESS_ERROR = -1,
    YANG_ZHANG_WITNESS_NO = 0,
    YANG_ZHANG_WITNESS_YES = 1
} YangZhangWitnessStatus;

typedef enum {
    YANG_ZHANG_EXTENSION_REFERENCE = 0,
    YANG_ZHANG_EXTENSION_OPTIMIZED = 1
} YangZhangExtensionSolver;

/*
 * Extend one exact Boolean assignment through the selected generic Wang
 * solver. The formula, reduction, and assignment are borrowed. The reduction
 * must be the successful yang_zhang_build() result for this exact formula;
 * only reduction.region is read, and the swap trace is ignored.
 *
 * This operation fixes only the three variable-gadget cells per variable. It
 * does not evaluate clauses, extract the returned witness, or compare the
 * result with the requested assignment. out_result must be zero-initialized
 * or previously destroyed. SAT and UNSAT preserve the generic WangSolveResult
 * ownership contract. If out_result already contains state, the call returns
 * ERROR and leaves that object unchanged; with a conforming destroyed output,
 * ERROR preserves the destroyed state.
 */
WangSolveStatus yang_zhang_solve_assignment_extension(
    const Cm13Formula *formula,
    const YangZhangReduction *reduction,
    const bool *assignment,
    size_t assignment_count,
    YangZhangExtensionSolver solver,
    WangSolveResult *out_result
);

/*
 * Verify a borrowed dense tiling and decode its exact variable-gadget
 * patterns. The formula/reduction provenance precondition is the same as
 * above. Clauses and reduction.swaps are not inspected.
 *
 * YES means the complete tiling is valid and every variable was decoded. NO
 * means the tiling is not a valid/decodable witness. ERROR means malformed API
 * storage, inconsistent detectable metadata, or allocation failure. Output is
 * transactional and receives exactly formula->variable_count values only on
 * YES.
 */
YangZhangWitnessStatus yang_zhang_extract_assignment(
    const Cm13Formula *formula,
    const YangZhangReduction *reduction,
    const TileId *tiling,
    size_t tiling_count,
    bool *out_assignment,
    size_t assignment_count
);

/*
 * Test only the representation relation between an assignment and a verified
 * dense tiling. YES and NO remain distinct from malformed-input ERROR. This
 * function does not decide whether the assignment satisfies the formula.
 */
YangZhangWitnessStatus yang_zhang_witnesses_correspond(
    const Cm13Formula *formula,
    const YangZhangReduction *reduction,
    const bool *assignment,
    size_t assignment_count,
    const TileId *tiling,
    size_t tiling_count
);

#endif /* WANG_YANG_ZHANG_WITNESS_H */
