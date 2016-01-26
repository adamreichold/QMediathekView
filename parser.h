#ifndef PARSER_H
#define PARSER_H

#include <functional>

#include "schema.h"

namespace Mediathek
{

using Inserter = std::function< void(const Show&) >;

bool parse(const QByteArray& data, const Inserter& inserter);

} // Mediatehk

#endif // PARSER_H
