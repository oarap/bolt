# Transition RowGroup Pruning to Expr Framework (RowVector Stats)

This plan implements `Expr`-based rowgroup pruning where statistics are wrapped in a `RowVector` and evaluated using the standard `exec::Expr` engine, while maintaining the per-rowgroup boolean check interface.

## Phase 1: Stats Expression Transformation

**Goal:** Transform filter expressions into pruning expressions that operate on statistics columns.

1. **Develop** **`StatsExprRewriter`**:

   * **Input**: Original Filter `exec::Expr` (operating on data columns).

   * **Output**: Pruning `exec::Expr` (operating on stats columns: `_min`, `_max`, `_null_count`).

   * **Rewriting Rules** (Targeting "Can Be True"):

     * `col < Lit` $\rightarrow$ `col_min < Lit`

     * `col > Lit` $\rightarrow$ `col_max > Lit`

     * `col = Lit` $\rightarrow$ `col_min <= Lit AND col_max >= Lit`

     * `IS NULL(col)` $\rightarrow$ `col_null_count > 0`

     * `IS NOT NULL(col)` $\rightarrow$ `col_null_count < col_total_count` (or `col_has_non_null` if available).

     * `AND`/`OR`: Recursively rewrite children.
2. **Schema Definition**:

   * Define the schema for the Stats Vector (e.g., `col_min`, `col_max`, `col_null_count`).

## Phase 2: ExprStatsEvaluator Implementation

**Goal:** Implement the evaluator that bridges `RowGroupStats` and `ExprSet`.

1. **Class Structure**:

   * `ExprStatsEvaluator` will hold:

     * `std::unique_ptr<exec::ExprSet>`: The compiled pruning expression.

     * `RowVectorPtr`: Reusable vector buffer for stats.
2. **`canBeTrue(const RowGroupStats& stats)`**:

   * **Step 1: Stats to Vector**: Populate the reusable `RowVector` (size 1) with values from `RowGroupStats` (Min, Max, NullCount).

   * **Step 2: Evaluation**: Call `ExprSet::eval` on this single-row vector.

   * **Step 3: Result**: Check the `SelectivityVector` (or result vector). If the row is selected, return `true`; otherwise `false`.

## Phase 3: Integration with ScanSpec

**Goal:** Use the new evaluator in the existing pruning flow.

1. **Update** **`ScanSpec`**:

   * Initialize `ExprStatsEvaluator` with the push-down filters.
2. **Modify** **`rowGroupMatches`**:

   * Delegate the check to `ExprStatsEvaluator::canBeTrue`.

   * Maintain backward compatibility options during the transition.

## Phase 4: Validation

* **Unit Tests**: Verify that `canBeTrue` returns correct results for various stats/filter combinations using the `RowVector` approach.

* **Correctness**: Ensure parity with the legacy `Filter` logic.
