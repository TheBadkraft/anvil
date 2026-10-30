# ANVL Software Licence (DRAFT 2610, for owner approval)

---

Copyright © 2025 Quantum Override. All rights reserved.

## 1. What this licence covers

This licence covers the ANVL software (Anvil Native, anvil.py, anvil.net, anvil.js and related components), including its source code, binaries, packages, headers, documentation and specifications (the "Software"). The Software is proprietary. Quantum Override owns it, and this licence gives you permission to use it only as set out below. All other rights are reserved.

## 2. Definitions

- **"Licensor"** means Quantum Override.
- **"You"** means the person or organization using the Software. If you use it for an organization, "You" means that organization.
- **"Organization"** means a company or other legal entity, together with every entity that controls it, is controlled by it, or is under common control with it.
- **"Developer seat"** means one person who writes, builds or maintains software that uses the Software. One person is one seat, however many machines they use.
- **"Personal use"** means use by an individual, for their own purposes, that is not commercial.
- **"Educational use"** means use by students, teachers and educational institutions for teaching, learning and non-commercial research.
- **"Commercial use"** means any use that is not personal use or educational use, including use by or for a business, a government body or any other organization, or in connection with making money.
- **"Your own machines and servers"** means computers that You own, rent or control, and that only You and Your own personnel use.

## 3. Free use

You may use the Software, free of charge, in these ways:

1. **Personal use.** Free and unlimited.
2. **Educational use.** Free and unlimited.
3. **Commercial use, up to 3 developer seats.** Free for an Organization with no more than 3 developer seats at any time.

Free use is perpetual, subject to section 5 (Termination).

## 4. Limits on free use

Free use is limited to Your own machines and servers. In addition, whether You use the Software for personal, educational or commercial purposes, You must not:

1. **Redistribute.** Copy, share, publish, sublicense, sell, lend, host for others to download, or otherwise make the Software available to anyone else, in any form. This covers the Software on its own and as part of anything else.
2. **Use it in browser apps.** Use the Software in any application that delivers the Software, or code or data derived from it, to a third party's browser or device.
3. **Use it in shipped products.** Use the Software in anything installed on, or delivered to, customers or other third parties.
4. **Modify it.** Modify the Software, or create derivative works from it.
5. **Reverse engineer it.** Decompile, disassemble or otherwise try to recover source code or internal designs of the Software, except to the extent applicable law expressly permits this despite this restriction.

Using the Software on Your own servers to run Your own service is allowed, as long as the Software itself is not delivered to a third party.

Redistribution, browser delivery and embedding in shipped products are available only under a commercial licence.

## 5. Commercial licences

If Your use goes beyond section 3 or section 4, You need a paid commercial licence from the Licensor. This includes:

- commercial use by an Organization with more than 3 developer seats;
- redistribution of the Software;
- use in browser apps; and
- use in shipped products.

Commercial licences are available. Contact the Licensor at [OWNER: contact email] for terms and pricing. Different components may be licensed on different terms.

Until You hold a commercial licence, You must stay within sections 3 and 4. An Organization that grows past 3 developer seats must obtain a commercial licence before the fourth seat starts using the Software.

## 6. Perpetual Author Grant

Quantum Override, Bandit Mobile Technologies, their present and future subsidiaries and affiliates (any entity that controls, is controlled by, or is under common control with either of them), their principals, and their successors and assigns, hold a perpetual, irrevocable, royalty-free, worldwide licence to use, modify, and distribute all Anvil components, including as part of any product or service, without restriction. The limits of this licence do not apply to them.

## 7. Ownership

The Software is licensed, not sold. This licence gives You no ownership of the Software and no right to use the names or trademarks of the Licensor.

## 8. Termination

If You breach this licence, Your rights under it end automatically. You must then stop using the Software and delete all copies. The Licensor may also give You written notice of a breach. Sections 6, 7 and 9 to 12 continue after termination. Ending this licence does not limit any other remedy available to the Licensor.

## 9. No warranty

THE SOFTWARE IS PROVIDED "AS IS" AND "AS AVAILABLE", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED. THIS INCLUDES ANY WARRANTY OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. THE LICENSOR DOES NOT WARRANT THAT THE SOFTWARE WILL BE ERROR-FREE OR UNINTERRUPTED.

## 10. Limitation of liability

TO THE FULLEST EXTENT PERMITTED BY LAW, THE LICENSOR WILL NOT BE LIABLE FOR ANY DAMAGES OR LOSSES ARISING FROM OR RELATED TO THE SOFTWARE OR THIS LICENCE, INCLUDING DIRECT, INDIRECT, INCIDENTAL, SPECIAL, CONSEQUENTIAL OR PUNITIVE DAMAGES, LOST PROFITS AND LOST DATA, WHATEVER THE LEGAL THEORY AND EVEN IF ADVISED OF THEIR POSSIBILITY. NOTHING IN THIS LICENCE LIMITS LIABILITY THAT CANNOT BE LIMITED BY LAW.

## 11. Governing law

This licence is governed by the laws of [OWNER: governing law], without regard to its conflict-of-laws rules. Courts in [OWNER: governing law / venue] have exclusive jurisdiction over disputes about it.

## 12. General

If a part of this licence is unenforceable, the rest stays in force. This licence is the whole agreement about the Software between You and the Licensor, unless You hold a separate commercial licence, which then takes priority for the use it covers. A failure to enforce a right is not a waiver of it.

## 13. Contact

Quantum Override: [OWNER: contact email]
Address (optional): [OWNER: postal address]

---

# Notes for the owner

**This is a draft, not legal advice. Have a lawyer review it before it is published, shipped in any package or relied on.**

## Placeholders to fill

1. `[OWNER: contact email]` (section 5 and section 13).
2. `[OWNER: governing law]` (section 11), the law that governs the licence.
3. `[OWNER: governing law / venue]` (section 11), the courts. Decide the venue separately from the law.
4. `[OWNER: postal address]` (section 13), optional.

## Judgement calls

- **Tier 1 / Tier 2 dropped.** Tier 1 named the parser `anvil.o` and Tier 2 named `anvil.security`, the AMP Secure Packet spec, the cipher registry and the `@amp.*` tag namespace. These are native-only, so the tiers do not read correctly for anvil.py, anvil.net or anvil.js. I replaced them with a generic "commercial licences are available" paragraph ending with "Different components may be licensed on different terms." Native-specific contents to hold for the commercial terms:
  - Tier 1 (Core): parser (`anvil.o`), all dialect support (AML, AMP, ASL), all public API headers; suited to embedding the parser in a product or service.
  - Tier 2 (Security add-on, requires Tier 1): `anvil.security` (`anvil.security.o`), the AMP Secure Packet specification, the cipher registry API, envelope validation, and use of the reserved `@amp.*` proposed tag namespace under compliance terms.
- **Perpetual Author Grant** uses your wording exactly. The old text said "in any product or service"; your text adds Bandit Mobile Technologies, affiliates, successors and assigns, and "modify and distribute". Note the grant says "all Anvil components", while the rest of the text says "the Software". The Software is defined to include the same components, but a lawyer should confirm the terms line up.
- **Free commercial use is per Organization, with affiliates counted together** (so a company cannot split into subsidiaries to get 3 seats each). This is my addition; you only said "per organization". Remove the affiliate wording in the "Organization" definition if you want each legal entity counted separately.
- **Seat counting.** A developer seat is anyone who writes, builds or maintains software that uses the Software, counted at any time. This includes contractors. I did not say how contractors are counted; a lawyer may want to.
- **Government and non-profit use** is treated as commercial use (any organization that is not personal or educational). Your decisions did not cover this; say if you want a free non-profit category.
- **Educational use** covers students, teachers and institutions for non-commercial research. Research funded by a company falls under commercial use.
- **Your own servers.** Running the Software on Your own server to provide a service is allowed on free terms provided the Software itself is not delivered to third parties. This follows your "free use is limited to the licensee's own machines and servers" and "browser apps" rules. SaaS use is a likely area for lawyer review. Machines rented from a cloud provider count as Your own.
- **Limits apply to all free users**, so the no-modification and no-redistribution rules also cover personal and educational users. Unlike a typical open licence, there is no right to patch the Software locally.
- **Reverse engineering** is prohibited except where law expressly permits it, as you specified.
- **Termination** is automatic on breach, with no cure period. Add one if you want to be gentler.
- **"Use" wording.** The old text forbade "use" without written permission. The new text grants free use and limits it, which resolves the contradiction.
- **Spelling.** "Licence" (British) is used throughout to match your brief; the copyright line is as specified. Tell me if you prefer "license" (US), especially since the npm field is `UNLICENSED`.
- **Identical for every package.** The text names all four packages and no native-only artifact. The npm `license` field should be `UNLICENSED`; for anvil.js, package metadata should point at the licence file. I changed none of that.

## Where the old text differs

- Old: "unauthorized copying, distribution, modification, or use ... strictly prohibited without express written permission". New: use is permitted free of charge under sections 3 and 4; only redistribution, modification and the other listed things are restricted.
- Old had `Contact: [TBD]` and `[TODO] Full commercial license terms to be published.` New has marked `[OWNER: ...]` placeholders only, and says commercial terms come by contacting the Licensor.
- Old Tier 1 and Tier 2 text is removed (see judgement calls).
- Old Perpetual Author Grant covered "Quantum Override and its principals ... to use". New covers Bandit Mobile Technologies, affiliates, successors and assigns, and adds modify and distribute, and the "limits do not apply to them" sentence.
- Added: definitions, free tiers (personal, educational, up to 3 developer seats), the five limits on free use, ownership, termination, no warranty, limitation of liability, governing law and general terms. None of these were in the old text.
- Old text said "proprietary and confidential". The "confidential" wording is dropped because the software is distributed publicly.
