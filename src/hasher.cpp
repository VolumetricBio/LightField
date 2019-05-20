#include "pch.h"

#include "hasher.h"

void Hasher::_hash( QString const fileName, QCryptographicHash::Algorithm const algorithm ) {
    QFile file { fileName };
    if ( file.open( QIODevice::ReadOnly ) ) {
        QCryptographicHash hasher { algorithm };
        hasher.addData( &file );
        file.close( );
        emit resultReady( { hasher.result( ).toHex( ) } );
    } else {
        debug( "+ Hasher::_hash: couldn't open file '%s'\n", fileName.toUtf8( ).data( ) );
        emit resultReady( QString( ) );
    }
}
