# Maintainer: Jesse McClure AKA "Trilby" <jmcclure [at] cns [dot] umass [dot] edu>
pkgname=iocane
pkgver=0.5
pkgrel=1
pkgdesc="Simulate X11 mouse events from keyboard"
url="http://github.com/TrilbyWhite/Iocane.git"
arch=('any')
license=('GPLv3')
depends=('libx11')
source=('https://github.com/TrilbyWhite/Iocane/raw/master/'$pkgname-$pkgver'.tar.gz')

build() {
    cd "${srcdir}"
	make
}

package() {
    cd "${srcdir}"
	make DESTDIR="${pkgdir}" install
}

