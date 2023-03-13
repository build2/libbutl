// file      : libbutl/host-os-release.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <string>
#include <vector>

#include <libbutl/optional.hxx>
#include <libbutl/target-triplet.hxx>

#include <libbutl/export.hxx>

namespace butl
{
  // Information extracted from /etc/os-release on Linux. See os-release(5)
  // for background. For other platforms we derive the equivalent information
  // from other sources. Some examples:
  //
  // {"debian", {}, "10", "",
  //  "Debian GNU/Linux", "buster", ""}
  //
  // {"fedora", {}, "35", "workstation",
  //  "Fedora Linux", "", "Workstation Edition"}
  //
  // {"ubuntu", {"debian"}, "20.04", "",
  //  "Ubuntu", "focal", ""}
  //
  // {"macos", {}, "12.5", "",
  //  "Mac OS", "", ""}
  //
  // {"freebsd", {}, "13.1", "",
  //  "FreeBSD", "", ""}
  //
  // {"windows", {}, "10", "",
  //  "Windows", "", ""}
  //
  // Note that for Mac OS, the version is the Mac OS version (as printed by
  // sw_vers) rather than Darwin version (as printed by uname).
  //
  // For Windows we currently do not distinguish the Server edition and the
  // version mapping is as follows:
  //
  // Windows 11             11
  // Windows 10             10
  // Windows 8.1            8.1
  // Windows 8              8
  // Windows 7              7
  // Windows Vista          6
  // Windows XP Pro/64-bit  5.2
  // Windows XP             5.1
  // Windows 2000           5
  //
  // Note that version_id may be empty, for example, on Debian testing:
  //
  // {"debian", {}, "", "",
  //  "Debian GNU/Linux", "", ""}
  //
  // Note also that we don't extract PRETTY_NAME because its content is
  // unpredictable. For example, it may include variant, as in "Fedora Linux
  // 35 (Workstation Edition)". Instead, construct it from the individual
  // components as appropriate, normally "$name $version ($version_codename)".
  //
  struct os_release
  {
    std::string              name_id;    // ID
    std::vector<std::string> like_ids;   // ID_LIKE
    std::string              version_id; // VERSION_ID
    std::string              variant_id; // VARIANT_ID

    std::string name;             // NAME
    std::string version_codename; // VERSION_CODENAME
    std::string variant;          // VARIANT
  };

  // Return the release information for the specified host or nullopt if the
  // specific host is unknown/unsupported. Throw std::runtime_error if
  // anything goes wrong.
  //
  // Note that "host" here implies that we may be running programs, reading
  // files, examining environment variables, etc., of the machine we are
  // running on.
  //
  LIBBUTL_SYMEXPORT optional<os_release>
  host_os_release (const target_triplet& host);
}
