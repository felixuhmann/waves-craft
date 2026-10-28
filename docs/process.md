# Clean-room reimplementation with AI and REA

Research checked: 28 October 2026. This is an engineering workflow synthesized from the sources below, not a legal opinion, a legal certification, or a workflow exercised in this research. No REA installation or target analysis was performed.

## Recommended reading

1. **RetroReversing: Legality of Reverse Engineering & Clean Room Reversing.** Best starting point for the methodology: one group analyzes and describes behavior; an independent group implements from that specification. It distinguishes clean-room work, same-team reimplementation and static recompilation, without claiming the label settles legality.[1]
2. **REA's official native-analysis guide.** Best first-party explanation of what REA actually provides: instructions, callers, constants and evidence for an agent to interpret. This is an analysis/reconstruction tutorial, not a legally reviewed clean-room protocol.[2]
3. **Kingy AI's worked REA tutorial.** Useful commands, evidence retention and compatibility checks, using a small public Electron example. Crucially, the author explicitly says they read the original source and that this is an educational reimplementation, not clean-room work. Do not mistake the passing comparisons for proof of legal independence.[3]
4. **EFF's Reverse Engineering FAQ.** Useful US-oriented legal risk checklist covering copyright, contracts, trade secrets and anti-circumvention. It is general guidance, not an opinion about a particular target.[4]

## The key distinction

Using REA to show an agent decompiled code and asking the same agent to rewrite it is not the separated implementation process described by the clean-room methodology source. Changing language or requesting different variable names does not establish independent implementation.[1]

The recommended information flow is:

```text
Licensed target and permitted observations
                |
         Analyst + REA
                |
      Private research evidence
                |
     Specification review gate
                |
 Approved behavioral spec and cleared fixtures
                |
    Isolated implementation agent/team
                |
       Independent verification
```

The steps below are my recommended adaptation of that separation to AI-assisted work. Separate agent sessions are an engineering control, not proof of legal equivalence to a traditional human clean-room team.

## 1. Define the permission boundary before analysis

Record the target, exact version, lawful access, relevant licence terms, applicable jurisdiction, intended compatibility goal and intended distribution. Start with public documentation and permitted black-box observation. Obtain target-specific review before relying on decompilation or circumventing technical protections. A clean-room specification cannot cure unlawfully obtained information.[4][5]

Prefer one narrow feature or interface over an entire commercial application.

## 2. Keep analysis in an analyst-only environment

Give the analyst the target and REA, with access limited to the work needed for the approved question. Keep binary hashes, tool versions, raw analysis, logs and uncertainty records privately on this side of the boundary. Do not give the implementation agent the original executable, extracted source, decompiler output, research transcript or vendor assets.

Use actual access controls, not just an instruction saying "do not look." Separate VMs or appropriately isolated users/mounts are preferable to two Git branches in a shared checkout. REA explicitly says it is not a sandbox and does not protect against malicious processes running as the same OS user.[6]

If a model provider will receive proprietary analysis, review that disclosure separately; local tool execution is not a promise that the agent's model calls remain local.

## 3. Review and publish a behavioral specification

For each feature, capture:

- Purpose, input/output types and units.
- State changes, defaults, supported ranges and error conditions.
- Observable ordering, timing or numerical tolerances needed for compatibility.
- User-created or otherwise cleared test fixtures and expected observations.
- Evidence references, confidence, and unresolved questions.

Exclude original code, disassembly, decompiler pseudocode, copied comments, proprietary artwork and a line-by-line translation of internal implementation structure. Derive the specification from cleared functional facts rather than mechanically paraphrasing the original implementation. Review fixture rights too: a test file or screenshot is not automatically cleared because it is useful for testing.

Freeze each approved specification revision so later results can be traced to exactly what the implementer received. For commercial distribution, have an IP lawyer review the information boundary and proposed release.

## 4. Implement from the approved package only

Use a fresh agent session and a workspace that can read only the approved specification, cleared fixtures, public references and its own implementation. Disable shared session history, cross-agent memory, transcript search and retrieval stores that could expose analysis. Do not mount the analyst's directory or give the builder its REA server.

When the builder has a question, route it through the same review gate. Resetting the chat while retaining the original analysis in memory, files or tool access does not establish separation.

## 5. Verify through an independent oracle

The verifier operates the licensed original within the approved permissions and compares it with the replacement on declared inputs. Return observed differences, not raw proprietary implementation details, to the builder. Keep original executable access and private analysis out of the builder's environment.

Test normal, boundary, invalid, stateful and failure cases. Record finite-case agreement as finite-case agreement, not universal equivalence. The worked REA tutorial makes this measurement boundary explicit.[3]

For an audio effect, my starting plan would use user-generated impulses, sweeps and level changes; compare frequency response, latency, parameter changes and stateful behavior. Use published DSP literature to implement the replacement. This is a proposed test strategy, not a claim that any particular Waves plugin has been measured or reproduced.

## 6. Retain provenance and audit the release

Keep exact target/tool versions, hashes, permission records, analyst evidence, approved-spec revisions, builder access configuration, implementation commits and test results. Review source history, packaged binaries, assets and third-party licences before publishing—not only the latest visible source tree.[1]

Document what was not inspected or reproduced. Clean-room separation addresses evidence of independent development; it is not a blanket defence to patent, trademark, contract or other claims.[4][5]

## Starter prompts

### Analyst — use only inside the approved analysis environment

> Investigate only the approved feature and only within the recorded permissions. Prefer public specifications and observable input/output behavior. Keep target binaries, extracted code and raw REA results in the analyst workspace. Produce a candidate behavioral specification covering inputs, outputs, state, errors, tolerances, evidence and unknowns. Do not include original code, disassembly, decompiler pseudocode or proprietary assets. Do not implement the replacement. Submit every proposed handoff and fixture to review.

### Builder — use only in the isolated implementation environment

> Implement only from the approved specification and cleared fixtures available in this workspace. Do not access the original product, analyst workspace, raw REA output or shared analysis history. Use independently sourced public references and record their provenance. If the specification is insufficient, submit a behavioral question through the review gate rather than requesting original implementation details. Test the declared contract and label unresolved cases.

These prompts describe desired behavior; access controls and review must enforce the boundary.

## EU/Austria caution

The EU Software Directive distinguishes observation from decompilation. Article 5(3) permits a person entitled to use a copy to observe, study or test its functioning during acts they are entitled to perform. Article 6's decompilation exception is specifically conditional on obtaining otherwise unavailable information indispensable for interoperability, is limited to necessary portions, and restricts subsequent use and disclosure. It is not a blanket permission to decompile any product to build a substitute. Article 8 also preserves other legal regimes. Austrian implementation and target-specific circumstances need separate review.[5]

**Recommendation:** begin with RetroReversing's methodology and REA's official technical guide, practice on an owned or suitably licensed open example, and use black-box observation first for a commercial target. Treat an AI clean-room pipeline as a documented engineering protocol, not a legal safety switch.

## Sources

[1] https://www.retroreversing.com/clean-room-reversing — Legality of Reverse Engineering & Clean Room Reversing - Retro Reversing (Reverse Engineering)
[2] https://rea.tools/guides/native — Native Binary Analysis — REA
[3] https://kingy.ai/blog/reverse-engineer-anything-ai-rea-guide — Reverse Engineer Anything with AI: REA Tested on a Notes Exporter - Kingy AI
[4] https://www.eff.org/issues/coders/reverse-engineering-faq — Coders’ Rights Project Reverse Engineering FAQ | Electronic Frontier Foundation
[5] https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32009L0024 — Directive - 2009/24 - EN - EUR-Lex
[6] https://raw.githubusercontent.com/morluto/rea/main/SECURITY.md — REA security policy
