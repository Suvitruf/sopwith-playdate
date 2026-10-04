# Provenance and distribution notes

This document records source evidence and release questions, not a determination that a future binary is cleared for every distribution channel.

## Upstream

Baseline: [SDL Sopwith revision c4e034109e28a5bd1727645fe77b27430219e8b5](https://github.com/fragglet/sdl-sopwith/tree/c4e034109e28a5bd1727645fe77b27430219e8b5), researched 2026-10-04.

The repository includes [GNU GPL version 2 in COPYING.md](https://github.com/fragglet/sdl-sopwith/blob/c4e034109e28a5bd1727645fe77b27430219e8b5/COPYING.md). Inspected engine file headers, including [swmain.c](https://github.com/fragglet/sdl-sopwith/blob/c4e034109e28a5bd1727645fe77b27430219e8b5/src/swmain.c), grant version 2 or later. Audit the notices of every imported component instead of inferring all file licenses from GitHub's repository label.

[AUTHORS](https://github.com/fragglet/sdl-sopwith/blob/c4e034109e28a5bd1727645fe77b27430219e8b5/AUTHORS) credits BMB Compuscience Canada, David L. Clark, Simon Howard, Christoph Reichenbach, and Jesse Smith. Preserve their notices and the original attribution when importing source or assets.

The engine is now imported under `Sources/core/`; see its [component inventory and modification ledger](../Sources/core/PORTING.md). Root `COPYING.md` preserves upstream GPL text. `font.h` uses LGPL-2.1-or-later, with license text in `LICENSES/LGPL-2.1.txt`; the unused Yocton parser retains its ISC notice. Newly authored port code, test code, CMake, and game build/reference scripts explicitly use GPL-2.0-or-later. Existing setup/probe/documentation files are not automatically assigned a different license by this import.

## Playdate SDK

The [SDK license](https://play.date/dev/sdk-license/) permits development using the SDK but restricts SDK redistribution, certain combinations with other licenses, and trademark use. Keep downloaded SDK tools, headers, documentation, and samples out of version control and release source archives. Reference the installed SDK from the build.

The current probe is original code using the API; SDK startup glue and libraries are referenced from the local installation at build time. The final game will need a component-level review of what is linked into its executable. Installing the SDK or separating folders does not by itself resolve GPL compatibility questions.

Do not put Panic's trademarks in the final application name without the permission required by the SDK terms. The repository's descriptive working title is not a decision about a release title or endorsement.

## Before publishing an engine derivative

- Preserve copyright/license notices and identify modified files and changes.
- Supply corresponding source and build instructions as required for the actual distribution, including the port and necessary scripts.
- Inventory linked runtime/startup components and any copied assets. Resolve the relationship between upstream GPL terms and SDK restrictions for that exact package; obtain clarification if needed rather than assuming an exception applies.
- Keep the SDK itself external. Check any separately bundled font, art, map, or audio licenses.
- Verify credits, product name, and release metadata. Review any additional distribution-channel terms separately.

These questions are a gate in the roadmap’s [release preparation](ROADMAP.md#4-prepare-the-release). Local research and port implementation can continue while the distribution approach is investigated. No public release or maintainer contact is part of the preparation task.
