#!/usr/bin/env python3
"""Write SPDX 2.3 SBOMs for the source tarball and the Linux packages.

  source  the release tarball, with the third-party code it bundles
  rpm     an installed rrdtool RPM, with the system packages its binaries
          load at run time
  deb     the same for an installed .deb

For rpm and deb the package must already be installed (the release jobs
install it to smoke-test it).  Every ELF file it installed is run through
ldd, and each shared library is mapped to the system package that owns it
(rpm -qf / dpkg -S), so the SBOM lists the exact versions the package was
built and tested against, including libraries only reached indirectly.  A
library that cannot be resolved, or is owned by no package, fails the
script rather than leaving it out.
"""

import argparse
import datetime
import hashlib
import json
import os
import re
import subprocess
import sys
import uuid

REPO_URL = "https://github.com/oetiker/rrdtool-1.x"
CREATOR = "Tool: rrdtool conftools/release_sbom.py"

RRDTOOL_LICENSE = {
    "licenseId": "LicenseRef-RRDtool",
    "name": "GPL-2.0-or-later with the RRDtool FLOSS License Exception",
    "extractedText": "GNU GPL version 2 or (at your option) any later version, "
                     "with the RRDtool FLOSS License Exception. See the COPYRIGHT file.",
}
PUBLIC_DOMAIN = {
    "licenseId": "LicenseRef-public-domain",
    "name": "Public domain",
    "extractedText": "Please do not copyright this code.  This code is in the public domain.",
}

# Third-party code carried in the source tree, by path in the tarball.
BUNDLED = [
    {"name": "compat-5.1", "version": "5.1r5", "path": "bindings/lua/compat-5.1r5",
     "license": "MIT", "copyright": "Copyright 2004-2006 The Kepler Project",
     "homepage": "https://keplerproject.github.io/compat/"},
    {"name": "strtod", "version": "NOASSERTION", "path": "src/rrd_strtod.c",
     "license": "BSD-3-Clause", "copyright": "Copyright (C) 2002 Michael Ringgaard",
     "homepage": "NOASSERTION"},
    {"name": "fnv", "version": "NOASSERTION", "path": "src/fnv.h",
     "license": PUBLIC_DOMAIN["licenseId"], "copyright": "NOASSERTION",
     "homepage": "http://www.isthe.com/chongo/tech/comp/fnv/"},
]


def checksums(path):
    sha1, sha256 = hashlib.sha1(), hashlib.sha256()
    with open(path, "rb") as fh:
        for block in iter(lambda: fh.read(1 << 16), b""):
            sha1.update(block)
            sha256.update(block)
    return [{"algorithm": "SHA1", "checksumValue": sha1.hexdigest()},
            {"algorithm": "SHA256", "checksumValue": sha256.hexdigest()}]


def spdx_id(*parts):
    return "SPDXRef-" + "-".join(re.sub(r"[^A-Za-z0-9.-]+", ".", p) for p in parts)


def run(*cmd):
    return subprocess.run(cmd, check=True, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, universal_newlines=True).stdout


def document(name, packages, relationships, licenses):
    return {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": name,
        "documentNamespace": f"{REPO_URL}/sbom/{name}-{uuid.uuid4()}",
        "creationInfo": {
            "creators": [CREATOR],
            "created": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        },
        "hasExtractedLicensingInfos": licenses,
        "packages": packages,
        "relationships": [{"spdxElementId": "SPDXRef-DOCUMENT", "relationshipType": "DESCRIBES",
                           "relatedSpdxElement": packages[0]["SPDXID"]}] + relationships,
    }


def rrdtool_package(spdxid, name, version, path):
    return {
        "name": name,
        "SPDXID": spdxid,
        "versionInfo": version,
        "packageFileName": os.path.basename(path),
        "downloadLocation": f"{REPO_URL}/releases/download/v{version}/{os.path.basename(path)}",
        "homepage": "https://oss.oetiker.ch/rrdtool/",
        "filesAnalyzed": False,
        "checksums": checksums(path),
        "licenseConcluded": "NOASSERTION",
        "licenseDeclared": RRDTOOL_LICENSE["licenseId"],
        "copyrightText": "Copyright (c) 1997-2019 Tobias Oetiker",
        "externalRefs": [{"referenceCategory": "PACKAGE-MANAGER", "referenceType": "purl",
                          "referenceLocator": f"pkg:github/oetiker/rrdtool-1.x@v{version}"}],
    }


def source_sbom(args):
    root = "SPDXRef-rrdtool"
    packages = [rrdtool_package(root, "rrdtool", args.version, args.tarball)]
    relationships = []
    for b in BUNDLED:
        pid = spdx_id("bundled", b["name"])
        packages.append({
            "name": b["name"],
            "SPDXID": pid,
            "versionInfo": b["version"],
            "downloadLocation": "NOASSERTION",
            "homepage": b["homepage"],
            "filesAnalyzed": False,
            "licenseConcluded": "NOASSERTION",
            "licenseDeclared": b["license"],
            "copyrightText": b["copyright"],
            "comment": f"Carried in the rrdtool source tree as {b['path']}.",
        })
        relationships.append({"spdxElementId": root, "relationshipType": "CONTAINS",
                              "relatedSpdxElement": pid})
    return document(os.path.basename(args.tarball), packages, relationships,
                    [RRDTOOL_LICENSE, PUBLIC_DOMAIN])


def os_release():
    info = {}
    with open("/etc/os-release", encoding="utf-8") as fh:
        for line in fh:
            key, _, value = line.strip().partition("=")
            info[key] = value.strip('"')
    return info["ID"], info.get("VERSION_ID", "")


def is_elf(path):
    try:
        with open(path, "rb") as fh:
            return fh.read(4) == b"\x7fELF"
    except OSError:
        return False


def shared_libraries(paths):
    """Resolved shared-library paths loaded by the given ELF files."""
    libs, missing = set(), []
    for path in paths:
        out = subprocess.run(["ldd", path], stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE, universal_newlines=True).stdout
        for line in out.splitlines():
            m = re.match(r"\s*(\S+) => (\S+)", line)
            if not m:
                continue
            if m.group(2) == "not":
                missing.append(f"{path}: {m.group(1)}")
            else:
                libs.add(os.path.realpath(m.group(2)))
    if missing:
        for item in missing:
            print(f"error: unresolved library {item}", file=sys.stderr)
        sys.exit(1)
    return libs


def rpm_info(args):
    fields = run("rpm", "-qp", "--qf", "%{NAME}\t%{VERSION}\t%{RELEASE}\t%{ARCH}", args.package).split("\t")
    files = run("rpm", "-qpl", args.package).split()
    return fields[0], f"{fields[1]}-{fields[2]}", fields[3], files


def deb_info(args):
    fields = run("dpkg-deb", "-f", args.package, "Package", "Version", "Architecture")
    name, version, arch = (line.split(": ", 1)[1] for line in fields.splitlines())
    files = [line.split()[-1].lstrip(".") for line in run("dpkg-deb", "-c", args.package).splitlines()]
    return name, version, arch, files


def rpm_owner(lib):
    out = subprocess.run(["rpm", "-qf", "--qf",
                          "%{NAME}\t%{EPOCHNUM}\t%{VERSION}-%{RELEASE}\t%{ARCH}\t%{LICENSE}\n", lib],
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                         universal_newlines=True)
    if out.returncode != 0:
        return None
    name, epoch, version, arch, lic = out.stdout.splitlines()[0].split("\t")
    return name, version, arch, lic, {"epoch": epoch} if epoch != "0" else {}


def deb_owner(lib):
    out = subprocess.run(["dpkg", "-S", lib], stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, universal_newlines=True)
    if out.returncode != 0:
        # usrmerge: dpkg may know the file under /lib rather than /usr/lib.
        alt = lib[4:] if lib.startswith("/usr/") else None
        if alt:
            out = subprocess.run(["dpkg", "-S", alt], stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, universal_newlines=True)
        if out.returncode != 0:
            return None
    # "libcairo2:amd64: /usr/lib/..." (several owners are comma-separated)
    pkg = out.stdout.splitlines()[0].rsplit(": ", 1)[0].split(",")[0].strip()
    q = run("dpkg-query", "-W", "-f", "${Package}\t${Version}\t${Architecture}\n", pkg).splitlines()[0]
    name, version, arch = q.split("\t")
    return name, version, arch, "NOASSERTION", {}


def package_sbom(args, kind):
    name, version, arch, files = rpm_info(args) if kind == "rpm" else deb_info(args)
    owner = rpm_owner if kind == "rpm" else deb_owner
    distro, distro_version = os_release()
    elves = sorted(f for f in files if os.path.isfile(f) and not os.path.islink(f) and is_elf(f))
    if not elves:
        sys.exit(f"no ELF files from {args.package} are installed; install the package first")
    own = {os.path.realpath(f) for f in files}

    root = spdx_id(name)
    packages = [rrdtool_package(root, name, args.version, args.package)]
    packages[0]["versionInfo"] = version
    packages[0]["externalRefs"].append({
        "referenceCategory": "PACKAGE-MANAGER", "referenceType": "purl",
        "referenceLocator": f"pkg:{kind}/{distro}/{name}@{version}?arch={arch}"
                            f"&distro={distro}-{distro_version}"})
    relationships, seen, unowned = [], {}, []
    for lib in sorted(shared_libraries(elves) - own):
        found = owner(lib)
        if not found:
            unowned.append(lib)
            continue
        dname, dversion, darch, dlicense, extra = found
        if dname in seen:
            continue
        qualifiers = "&".join(f"{k}={v}" for k, v in
                              [("arch", darch), *extra.items(), ("distro", f"{distro}-{distro_version}")])
        pid = spdx_id(kind, dname)
        seen[dname] = pid
        packages.append({
            "name": dname,
            "SPDXID": pid,
            "versionInfo": dversion,
            "supplier": "NOASSERTION",
            "downloadLocation": "NOASSERTION",
            "filesAnalyzed": False,
            "licenseConcluded": "NOASSERTION",
            "licenseDeclared": "NOASSERTION",
            "copyrightText": "NOASSERTION",
            "comment": f"Declared license (distribution metadata): {dlicense}"
                       if dlicense != "NOASSERTION" else "Shared library loaded at run time.",
            "externalRefs": [{"referenceCategory": "PACKAGE-MANAGER", "referenceType": "purl",
                              "referenceLocator": f"pkg:{kind}/{distro}/{dname}@{dversion}?{qualifiers}"}],
        })
        relationships.append({"spdxElementId": root, "relationshipType": "DEPENDS_ON",
                              "relatedSpdxElement": pid})
    if unowned:
        for lib in unowned:
            print(f"error: {lib} is not owned by any {kind} package", file=sys.stderr)
        sys.exit(1)
    return document(os.path.basename(args.package), packages, relationships,
                    [RRDTOOL_LICENSE])


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    # AlmaLinux 8 ships Python 3.6, before add_subparsers(required=True).
    sub = parser.add_subparsers(dest="kind")
    sub.required = True
    src = sub.add_parser("source", help="SBOM for the source tarball")
    src.add_argument("--tarball", required=True)
    for kind in ("rpm", "deb"):
        p = sub.add_parser(kind, help=f"SBOM for an installed {kind} package")
        p.add_argument("--package", required=True, help=f"the .{kind} file that is installed")
    for p in sub.choices.values():
        p.add_argument("--version", required=True, help="rrdtool version, e.g. 1.11.1")
        p.add_argument("--output", required=True, help="SBOM file to write")
    args = parser.parse_args()

    sbom = source_sbom(args) if args.kind == "source" else package_sbom(args, args.kind)
    with open(args.output, "w", encoding="utf-8") as fh:
        json.dump(sbom, fh, indent=2)
        fh.write("\n")
    print(f"{args.output}: {len(sbom['packages'])} packages")


if __name__ == "__main__":
    main()
