#ifndef GSEC_PROTO_H
#define GSEC_PROTO_H

#include "../common/ThreadData.h"
#include "../common/UtilSvc.h"

namespace Firebird::Gsec
{


// Output reporting utilities
void	GSEC_print_status(const ISC_STATUS*);
void	GSEC_error_redirect(const ISC_STATUS*, USHORT);
void	GSEC_error(USHORT code, const ISC_STATUS* status = NULL);
void	GSEC_exit();
void	GSEC_print(USHORT, const char* str = NULL);
void	GSEC_message(USHORT, const char* str = NULL);
void	GSEC_print_partial(USHORT);
void	GSEC_diag(USHORT);

int		gsec(Firebird::UtilSvc*);
int		GSEC_main(Firebird::UtilSvc*);


} // namespace Firebird::Gsec

#endif // GSEC_PROTO_H
