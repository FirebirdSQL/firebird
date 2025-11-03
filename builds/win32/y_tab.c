
//
// @(#)btyaccpar, based on byacc 1.8 (Berkeley)
// Parser skeleton modified for use in the Firebird project by Nickolay Samofatov
//
#define YYBTYACC 1

#include "firebird.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <optional>
#include "../dsql/Nodes.h"
#include "../dsql/AggNodes.h"
#include "../dsql/DdlNodes.h"
#include "../dsql/BoolNodes.h"
#include "../dsql/ExprNodes.h"
#include "../dsql/PackageNodes.h"
#include "../dsql/StmtNodes.h"
#include "../dsql/WinNodes.h"
#include "../jrd/RecordSourceNodes.h"
#include "../common/classes/TriState.h"
#include "gen/parse.h"
#include "../dsql/Parser.h"

