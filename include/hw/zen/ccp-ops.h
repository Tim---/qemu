#ifndef CCP_OPS_H
#define CCP_OPS_H

int ccp_op_passthru(CcpState *s, struct ccp5_desc * desc);
int ccp_op_aes(CcpState *s, struct ccp5_desc * desc);
int ccp_op_rsa(CcpState *s, struct ccp5_desc * desc);
int ccp_op_sha(CcpState *s, struct ccp5_desc * desc);
int ccp_op_zlib(CcpState *s, struct ccp5_desc * desc);

#endif
