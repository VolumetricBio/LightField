#ifndef __STRINGS_H__
#define __STRINGS_H__

//
// Functions for converting various types to constant strings.
//

char const* ToString( bool                          const value );

char const* ToString( QDialog::DialogCode           const value );

char const* ToString( QProcess::ProcessError        const value );
char const* ToString( QProcess::ProcessState        const value );
char const* ToString( QProcess::ExitStatus          const value );

char const* ToString( QSwipeGesture::SwipeDirection const value );

char const* ToString( Qt::GestureState              const value );

//
// Functions for converting various types to variable strings.
//

QString     ToString( QPoint                        const value );
QString     ToString( QRect                         const value );
QString     ToString( QSize                         const value );

QString     ToString( QPointF                       const value );
QString     ToString( QRectF                        const value );
QString     ToString( QSizeF                        const value );

//
// Other string-related functions.
//

QString FormatDouble( double const value, int const fieldWidth = 0, int const precision = -1 );

QString GroupDigits( QString const& input, char const groupSeparator );

#endif // __STRINGS_H__
