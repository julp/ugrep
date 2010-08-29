# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=3

inherit versionator cmake-utils

COMMIT="12deca3"

S="${WORKDIR}/julp-ugrep-${COMMIT}"

AVC=( $(get_version_components) )
MAJOR_VER="${AVC[0]}"
MINOR_VER="${AVC[1]}"
DESCRIPTION="ICU based implementation of grep to deal with differents codepages/encodings"
HOMEPAGE="http://github.com/julp/ugrep"
LICENSE="BSD"
IUSE="debug"
SLOT="0"
KEYWORDS="~x86"
SRC_URI="http://download.github.com/julp-ugrep-${MAJOR_VER}.${MINOR_VER}-0-g${COMMIT}.tar.gz"
DEPEND=">=dev-libs/icu-4
	bzip2? ( app-arch/bzip2 )
	zlib? ( sys-libs/zlib )"

src_configure() {
	local mycmakeargs
	use debug && mycmakeargs="-DDEBUG=ON"
	epatch "${FILESDIR}"/ugrep-0.3-CMakeLists.patch
	cmake-utils_src_configure
}

#src_install() {
#    emake -j1 DESTDIR="${D}" install || die "emake install failed"
#}
