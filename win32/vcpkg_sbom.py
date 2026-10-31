#!/usr/bin/env python3
"""Write an SPDX 2.3 SBOM for a collected Windows release directory.

The release zip ships rrdtool's own binaries plus third-party DLLs copied
out of vcpkg. vcpkg records every installed port in
<installed>/share/<port>/vcpkg.spdx.json, including a SHA-256 for each file
the port installed. Every DLL in the release directory is matched to the
port that installed it by name and hash, so the SBOM lists exactly the
ports whose code ships, at the versions vcpkg built. A DLL that matches no
port, or whose hash differs from the vcpkg record, fails the script: the
SBOM would otherwise misdescribe the zip.
"""

import argparse
import datetime
import glob
import hashlib
import json
import os
import re
import sys
import uuid

OWN_DLLS = {"librrd-8.dll"}
REPO_URL = "https://github.com/oetiker/rrdtool-1.x"


def digests(path):
    """Return (sha1, sha256); SPDX 2.3 requires SHA1 on every file entry."""
    sha1, sha256 = hashlib.sha1(), hashlib.sha256()
    with open(path, "rb") as fh:
        for block in iter(lambda: fh.read(1 << 16), b""):
            sha1.update(block)
            sha256.update(block)
    return sha1.hexdigest(), sha256.hexdigest()


def spdx_id(*parts):
    return "SPDXRef-" + "-".join(re.sub(r"[^A-Za-z0-9.-]+", ".", p) for p in parts)


def load_ports(installed):
    ports = {}
    pattern = os.path.join(installed, "share", "*", "vcpkg.spdx.json")
    for path in sorted(glob.glob(pattern)):
        with open(path, encoding="utf-8") as fh:
            doc = json.load(fh)
        port = os.path.basename(os.path.dirname(path))
        ports[port] = doc
    if not ports:
        sys.exit(f"no vcpkg.spdx.json found under {pattern}")
    return ports


def index_dlls(ports):
    """Map lower-case DLL file name to (port, file entry) from vcpkg's records."""
    dlls = {}
    for port, doc in ports.items():
        for entry in doc.get("files", []):
            name = entry.get("fileName", "")
            if not entry.get("SPDXID", "").startswith("SPDXRef-binary-file"):
                continue
            if not (name.startswith("./bin/") and name.lower().endswith(".dll")):
                continue
            key = os.path.basename(name).lower()
            if key in dlls and dlls[key][0] != port:
                sys.exit(f"{key} is installed by both {dlls[key][0]} and {port}")
            dlls[key] = (port, entry)
    return dlls


def recorded_sha256(entry):
    for checksum in entry.get("checksums", []):
        if checksum.get("algorithm") == "SHA256":
            return checksum.get("checksumValue", "").lower()
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--installed", required=True,
                        help="vcpkg installed tree for the triplet, e.g. vcpkg/installed/x64-windows")
    parser.add_argument("--release-dir", required=True,
                        help="directory produced by collect_rrdtool_vcpkg_files.bat")
    parser.add_argument("--version", required=True, help="rrdtool version, e.g. 1.11.1")
    parser.add_argument("--name", required=True, help="artifact name, e.g. rrdtool-1.11.1-x64_vcpkg")
    parser.add_argument("--output", required=True, help="SBOM file to write")
    args = parser.parse_args()

    ports = load_ports(args.installed)
    dll_index = index_dlls(ports)

    root = "SPDXRef-rrdtool"
    packages = [{
        "name": "rrdtool",
        "SPDXID": root,
        "versionInfo": args.version,
        "downloadLocation": f"git+{REPO_URL}@v{args.version}",
        "homepage": "https://oss.oetiker.ch/rrdtool/",
        "licenseConcluded": "NOASSERTION",
        "licenseDeclared": "LicenseRef-RRDtool",
        "copyrightText": "Copyright (c) 1997-2019 Tobias Oetiker",
        "externalRefs": [{
            "referenceCategory": "PACKAGE-MANAGER",
            "referenceType": "purl",
            "referenceLocator": f"pkg:github/oetiker/rrdtool-1.x@v{args.version}",
        }],
    }]
    files = []
    relationships = [{
        "spdxElementId": "SPDXRef-DOCUMENT",
        "relationshipType": "DESCRIBES",
        "relatedSpdxElement": root,
    }]

    used = {}
    unmatched = []
    mismatched = []
    for path in sorted(glob.glob(os.path.join(args.release_dir, "*"))):
        base = os.path.basename(path)
        lower = base.lower()
        if not (lower.endswith(".dll") or lower.endswith(".exe")):
            continue
        sha1, digest = digests(path)
        if lower.endswith(".exe") or lower in OWN_DLLS:
            file_id = spdx_id("rrdtool", "file", base)
            owner = root
        elif lower in dll_index:
            port, entry = dll_index[lower]
            expected = recorded_sha256(entry)
            if expected != digest:
                mismatched.append(f"{base}: vcpkg {expected}, release {digest}")
                continue
            used[port] = True
            file_id = spdx_id(port, "file", base)
            owner = spdx_id(port, "binary")
        else:
            unmatched.append(base)
            continue
        files.append({
            "fileName": "./" + base,
            "SPDXID": file_id,
            "checksums": [
                {"algorithm": "SHA1", "checksumValue": sha1},
                {"algorithm": "SHA256", "checksumValue": digest},
            ],
            "licenseConcluded": "NOASSERTION",
            "copyrightText": "NOASSERTION",
        })
        relationships.append({
            "spdxElementId": owner,
            "relationshipType": "CONTAINS",
            "relatedSpdxElement": file_id,
        })

    if unmatched or mismatched:
        for base in unmatched:
            print(f"error: {base} was not installed by any vcpkg port", file=sys.stderr)
        for line in mismatched:
            print(f"error: hash mismatch for {line}", file=sys.stderr)
        sys.exit(1)

    licenses = {"LicenseRef-RRDtool": {
        "licenseId": "LicenseRef-RRDtool",
        "name": "GPL-2.0-or-later with the RRDtool FLOSS License Exception",
        "extractedText": "GNU GPL version 2 or (at your option) any later version, "
                         "with the RRDtool FLOSS License Exception. See the COPYRIGHT file.",
    }}
    for port in sorted(used):
        doc = ports[port]
        for info in doc.get("hasExtractedLicensingInfos", []):
            licenses.setdefault(info["licenseId"], info)
        # vcpkg's own IDs (SPDXRef-port, SPDXRef-binary, SPDXRef-resource-N)
        # repeat in every port document; prefix them with the port name.
        rename = {p["SPDXID"]: spdx_id(port, p["SPDXID"][len("SPDXRef-"):])
                  for p in doc.get("packages", [])}
        for pkg in doc.get("packages", []):
            copy = dict(pkg)
            copy["SPDXID"] = rename[pkg["SPDXID"]]
            packages.append(copy)
        for rel in doc.get("relationships", []):
            src, dst = rel.get("spdxElementId"), rel.get("relatedSpdxElement")
            if src in rename and dst in rename:
                relationships.append({
                    "spdxElementId": rename[src],
                    "relationshipType": rel["relationshipType"],
                    "relatedSpdxElement": rename[dst],
                })
        relationships.append({
            "spdxElementId": root,
            "relationshipType": "CONTAINS",
            "relatedSpdxElement": spdx_id(port, "binary"),
        })

    # vcpkg writes LicenseRef-vcpkg-null for a port that declares no
    # license, without defining it; an undefined reference makes the
    # document invalid, so it becomes NOASSERTION.
    for pkg in packages:
        for field in ("licenseConcluded", "licenseDeclared"):
            value = pkg.get(field, "")
            refs = re.findall(r"LicenseRef-[A-Za-z0-9.-]+", value)
            if any(ref not in licenses for ref in refs):
                pkg[field] = "NOASSERTION"

    sbom = {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": args.name,
        "documentNamespace": f"{REPO_URL}/sbom/{args.name}-{uuid.uuid4()}",
        "creationInfo": {
            "creators": ["Tool: rrdtool win32/vcpkg_sbom.py"],
            "created": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        },
        "hasExtractedLicensingInfos": list(licenses.values()),
        "packages": packages,
        "files": files,
        "relationships": relationships,
    }
    with open(args.output, "w", encoding="utf-8") as fh:
        json.dump(sbom, fh, indent=2)
        fh.write("\n")
    print(f"{args.output}: {len(files)} files, {len(used)} vcpkg ports")


if __name__ == "__main__":
    main()
