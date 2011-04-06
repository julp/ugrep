# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

inherit git cmake-utils

EAPI=3

DESCRIPTION="ICU based implementation of grep to deal with differents codepages/encodings"
EGIT_REPO_URI="git://github.com/julp/ugrep.git"
EGIT_BRANCH="master"
HOMEPAGE="http://github.com/julp/ugrep"
SRC_URI=""
LICENSE="BSD"
IUSE="+bzip2 +zlib debug"
SLOT="0"
KEYWORDS="~x86"
DEPEND=">=dev-libs/icu-4
	bzip2? ( app-arch/bzip2 )
	zlib? ( sys-libs/zlib )"

src_unpack() {
	git_src_unpack
}

src_configure() {
	use debug && CMAKE_BUILD_TYPE="Maintainer"
	cmake-utils_src_configure
}
