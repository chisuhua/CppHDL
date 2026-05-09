HANDOFF CONTEXT
===============

USER REQUESTS (AS-IS)
---------------------
- "请检查docs/adr目录下的文档，并对[Pasted ~112...]"
- "请评审现有的 ADR, PRD文档"
- "请修改和完善"
- "请列出我们需要讨论的其他ADR议题"
- "按你建议的顺序进行讨论，现在给出第一个议题进行讨论"
- "按你建议进行讨论"
- "同意" (multiple times)
- "选B，ADR要保留我们的讨论的过程，以便后期可能对决议进行审查"
- "同意这个方案，请先记录你的分析到该决策文档里， 然后我们继续讨论这个决策的下一个问题点"
- "同意" (on Q2 recommendation)
- "请分析Q1为什么会有这么多组合的设计？是因为不同层次模块直接的链接，链接的驱动可能是输入端口，也可能是输出端口吗？set_driver和set_src的区别实施什么"
- "同意，上面的分析过程需要记录到ADR决议文档中。继续Q2"
- "同意" (on Q1+Q2)
- "选择A"
- "选择A, 但是加入TODO的注释"
- "继续" (multiple times, to proceed to next topics)
- "/handoff"

GOAL
----
Continue the ADR discussion series. The user and I have been systematically discussing architecture decisions through pre-defined topics. We had just started topic #6 (zero_cache/ones_cache thread safety) when the handoff was triggered. The next step is to continue discussing and resolving topic #6, then proceed through the remaining 23 topics in priority order.

WORK COMPLETED
--------------
- Reviewed all 7 existing ADR/PRD documents and issued a structured review with cross-document consistency analysis
- Performed cross-document consistency fixes across ADR-003, ADR-005, ADR-004, ADR-001, ADR-006, and PRD (CALL_EXTERNAL dual narrative, A/B verification status, PRD timeline, cross-references)
- Created ADR-DISCUSSION-PLAN.md tracking 29 topics across P0-P3 priority levels
- Discussed and resolved 5 architecture topics, creating 4 ADR documents:
  * Topic #1 (Asymmetric memory model) -> ADR-007 sections 2-5. Decisions: remove destruction_manager, clear_sources cleans both srcs_ and users_, Simulator lifecycle shorter than context as hard constraint, no special exception safety handling
  * Topic #2 (data_map_ shared mutable state) -> ADR-007 section 6. Decisions: change unordered_map to vector, accept O(4N) JIT sync with lightweight dirty flag, keep lifecycle as-is, keep DAG coupling as-is
  * Topic #3 (Single clock domain constraint) -> ADR-008. Decisions: no CDC support, async FIFO stays commented out, document constraint in code
  * Topic #4 (Simulation evaluation order) -> ADR-009. Decisions: use edge detection instead of level-based, replace dual eval() with edge-triggered model, keep input classification heuristic as-is
  * Topic #5 (<<= connection semantics) -> ADR-010. Decisions: unify set_driver/set_src to set_src, replace dynamic_cast with static_cast+assert, replace TODO with CHREQUIRE, keep operator&&/|| with TODO markers
- Code modifications: clear_sources() cleanup (lnodeimpl.h), destruction_manager removal (context.cpp), CDC unsupported annotations (context.h, fifo.h), operator&&/|| TODO comments (io.h)

CURRENT STATE
-------------
- 5 of 29 topics completed, 24 remaining
- All completed topics have corresponding ADR documents (ADR-007 through ADR-010)
- Existing ADRs ADR-001 through ADR-006 were draft/completed before this session and were reviewed/fixed (consistency updates applied)
- All changes are uncommitted (working tree has modifications to 13 files)
- The git repo has moved ADR files from docs/architecture/decisions/ to docs/adr/
- A new ADR discussion plan is at docs/adr/ADR-DISCUSSION-PLAN.md

PENDING TASKS
-------------
- Topic #6 (zero_cache/ones_cache thread safety): In progress. My recommendation was Option D (small width <=64 no cache + large width thread_local cache). Waiting for user decision.
- Topics #7-28: All pending discussion
- Topics #7-28 cover: IO port dual API, type hierarchy, Bundle reflection, topological sort as single IR, ch_reg<T> inheritance, double in_static_destruction(), Verilog codegen completeness, DAG codegen positioning, JIT operation coverage, SpinalHDL stream gaps, simulation primitives, BlackBox/IP, formal verification, CH_MODULE macro, LLVM-18 hardcoding, disabled tests, naming conventions, include guard style, JIT ch_op-JitOp sync, context_interface dead code, namespace pollution, component build non-idempotency
- Implementation tasks from ADR decisions (not yet coded):
  * ADR-007 Q1: data_map_ vector migration (8-step plan)
  * ADR-009 D1: Edge detection tick() implementation (PRD T1)
  * ADR-010 Q1/Q2: set_src unification + static_cast replacement in operator<<=
  * ADR-010 Q3: CHREQUIRE replacement for TODO comments in io.h

KEY FILES
---------
- docs/adr/ADR-DISCUSSION-PLAN.md - Master plan tracking 29 topics across priorities
- docs/adr/ADR-007-memory-ownership-model.md - Topics #1 and #2 decisions
- docs/adr/ADR-008-single-clock-domain-constraint.md - Topic #3 decisions
- docs/adr/ADR-009-simulation-evaluation-order.md - Topic #4 decisions
- docs/adr/ADR-010-connection-semantics.md - Topic #5 decisions
- include/core/types.h - zero_cache location (topic #6 discussion, line 176)
- src/utils/types.cpp - ones_cache location (topic #6 discussion, line 446)
- include/core/lnodeimpl.h - clear_sources() modified (D2 implementation)
- src/core/context.cpp - destruction_manager cleaned up (D1 implementation)
- include/core/io.h - operator<<= overloads (topic #5)

IMPORTANT DECISIONS
-------------------
- ADR format must preserve full discussion process with analysis, options considered, and reasoning. This is for future review of past decisions.
- Discussion procedure: Present status/context -> discuss options -> reach decision -> record in ADR document. Code changes only when explicitly requested.
- Topic #6 recommendation: Option D (small width <=64 no cache, large width thread_local cache) for zero()/ones() factory functions
- All ADR documents go under docs/adr/ with filename prefix ADR-NNN-
- 29 topics were categorized from exhaustive codebase analysis: 7 P0, 6 P1, 7 P2, 9 P3

EXPLICIT CONSTRAINTS
--------------------
None.

CONTEXT FOR CONTINUATION
------------------------
- This session spanned 5 ADR topics. The discussion methodology: I present analysis with code evidence, user evaluates options, I give recommendations, user agrees/chooses, I record in ADR.
- ADR-DISCUSSION-PLAN.md at docs/adr/ADR-DISCUSSION-PLAN.md is the single source of truth for what has been done and what remains. Load it first when continuing.
- When continuing, start by loading docs/adr/ADR-DISCUSSION-PLAN.md to see current state, then continue with topic #6 (zero_cache/ones_cache) where my recommendation of Option D was awaiting user decision.
- For topic #6: zero() is in include/core/types.h:176, ones() is in src/utils/types.cpp:446. Both have thread_local commented out. Core question is whether to restore thread_local, add mutex, remove cache, or use hybrid approach (Option D).
- All completed topics have ADR documents that record full analysis. Start with ADR-DISCUSSION-PLAN.md for the summary, then read specific ADR-NNN documents for detailed decisions.
- Code changes from this session are uncommitted. git status shows modifications to lnodeimpl.h, context.cpp, context.h, fifo.h, io.h plus new ADR documents in docs/adr/.
