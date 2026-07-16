# License Compliance Approval

Status: `REVIEW REQUIRED BEFORE PUBLIC RELEASE`

Date prepared: 2026-07-16

## Technical Inventory Confirmed

- Qt 6.10.3 libraries and plugins are dynamically linked.
- Qt license texts are included in the runtime package.
- LLVM-MinGW `libc++.dll` and `libunwind.dll` are accompanied by the Apache-2.0 WITH LLVM-exception text.
- Microsoft and software OpenGL deployment files are inventoried with official redistribution references.
- Current uncleared character projects are excluded from the public runtime package.
- The public beta application icon is original geometric project artwork with recorded SHA-256 values and redistribution permission.

## Required Release-Owner Approval

The release owner must select and document the actual Qt licensing route:

- a valid commercial Qt license; or
- compliance with the applicable LGPL/GPL obligations for the dynamically linked distribution.

The release owner must also confirm the redistribution terms for the exact Microsoft and software OpenGL files in the final artifact.

## Approval

- Release owner: `NOT SIGNED`
- Legal/compliance reviewer: `NOT SIGNED`
- Approval date: `NOT CONFIGURED`
- Approved artifact SHA-256: `NOT CONFIGURED`

This document is a technical compliance checklist, not legal advice. Until the approval fields are completed, license-route approval remains `MANUAL TEST`.
