#include <pmath-builtins/all-symbols-private.h>

#include <pmath-core/symbols-private.h>

#include <pmath-util/concurrency/atomic-private.h>
#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/debug.h>
#include <pmath-util/hash/hashtables-private.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/security.h>
#include <pmath-util/symbol-values-private.h>

#include <string.h>


#define ASSOCIATIVE           PMATH_SYMBOL_ATTRIBUTE_ASSOCIATIVE
#define DEEPHOLDALL           PMATH_SYMBOL_ATTRIBUTE_DEEPHOLDALL
#define DEFINITEFUNCTION      PMATH_SYMBOL_ATTRIBUTE_DEFINITEFUNCTION
#define HOLDALL               PMATH_SYMBOL_ATTRIBUTE_HOLDALL
#define HOLDALLCOMPLETE       PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE
#define HOLDFIRST             PMATH_SYMBOL_ATTRIBUTE_HOLDFIRST
#define HOLDREST              PMATH_SYMBOL_ATTRIBUTE_HOLDREST
#define LISTABLE              PMATH_SYMBOL_ATTRIBUTE_LISTABLE
#define NHOLDALL              PMATH_SYMBOL_ATTRIBUTE_NHOLDALL
#define NHOLDFIRST            PMATH_SYMBOL_ATTRIBUTE_NHOLDFIRST
#define NHOLDREST             PMATH_SYMBOL_ATTRIBUTE_NHOLDREST
#define NUMERICFUNCTION       PMATH_SYMBOL_ATTRIBUTE_NUMERICFUNCTION
#define ONEIDENTITY           PMATH_SYMBOL_ATTRIBUTE_ONEIDENTITY
#define READPROTECTED         PMATH_SYMBOL_ATTRIBUTE_READPROTECTED
#define SEQUENCEHOLD          PMATH_SYMBOL_ATTRIBUTE_SEQUENCEHOLD
#define SYMMETRIC             PMATH_SYMBOL_ATTRIBUTE_SYMMETRIC
#define THREADLOCAL           PMATH_SYMBOL_ATTRIBUTE_THREADLOCAL


#define PMATH_DECLARE_SYMBOL(SYM, NAME, ATTRIB)   PMATH_PRIVATE pmath_symbol_t SYM = PMATH_STATIC_NULL;
#  include "symbols.inc"
#undef PMATH_DECLARE_SYMBOL


/*============================================================================*/

//{ builtins from src/pmath-builtins/arithmetic/ ...
PMATH_PRIVATE pmath_t builtin_abs(                  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_approximate(          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_arg(                  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_binomial(             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_chop(                 pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_complex(              pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_conjugate(            pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_denominator(          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_directedinfinity(     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_exp(                  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_factorial(            pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_factorial2(           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_fractionalpart(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_gamma(                pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_im(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_log(                  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_loggamma(             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_mod(                  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_numerator(            pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_numerator_denominator(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_plus(                 pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_polygamma(            pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_power(                pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_powermod(             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_precision(            pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_product(              pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_quotient(             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_re(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_rescale(              pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_round_functions(      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_setprecision(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_sign(                 pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_sqrt(                 pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_sum(                  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_times(                pmath_expr_t expr);

PMATH_PRIVATE pmath_bool_t builtin_approximate_e(               pmath_t *obj, double prec);
PMATH_PRIVATE pmath_bool_t builtin_approximate_eulergamma(      pmath_t *obj, double prec);
PMATH_PRIVATE pmath_bool_t builtin_approximate_machineprecision(pmath_t *obj, double prec);
PMATH_PRIVATE pmath_bool_t builtin_approximate_pi(              pmath_t *obj, double prec);
PMATH_PRIVATE pmath_bool_t builtin_approximate_power(           pmath_t *obj, double prec);

PMATH_PRIVATE pmath_t builtin_assign_maxextraprecision(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_assign_setprecision(     pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_internal_copysign(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_internal_nexttoward(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_internal_realballfrommidpointradius(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_internal_realballbounds(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_internal_realballmidpointradius(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_internal_signbit(pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/control/ ...
PMATH_PRIVATE pmath_t builtin_developer_hasassignedrules(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_developer_hasbuiltincode(  pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_isheld(          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_call_isheld(     pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_bytecount(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_count(           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_filterrules(     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_hash(            pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_history(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_loadlibrary(     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_match(           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_memoryusage(     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_position(        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_releasehold(     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_replace(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_replacelist(     pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_private_isvalid( pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_private_setvalid(pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/control/definitions/ ...
PMATH_PRIVATE pmath_t builtin_assign(   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_tagassign(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_unassign(   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_tagunassign(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_assignwith(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_dec_or_inc_or_postdec_or_postinc(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_divideby_or_timesby(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_clear_or_clearall(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_update(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_assign_list(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_assign_ownrules(    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_assign_symbol_rules(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_ownrules(    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_symbol_rules(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_assign_attributes(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_attributes(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_clearattributes(  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_setattributes(    pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_protect_or_unprotect(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_assign_default(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_default(       pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_assign_options(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isoption(      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_options(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_optionvalue(   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_setoptions(    pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_showdefinition(pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/control/flow/ ...
PMATH_PRIVATE pmath_t builtin_internal_abortmessage(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_throw(                pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_catch(                pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_finally(              pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_evaluate(          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_evaluatedelayed(   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_evaluationsequence(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_internal_maketrustedfunction(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_internal_tryevaluatesecured(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_function(            pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_call_function(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_trustedfunction(     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_call_trustedfunction(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_block(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_internal_blockuserdefinitions(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_local(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_with(                         pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_if(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_do(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_for(      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_piecewise(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_switch(   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_while(    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_which(    pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_begin(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_beginpackage(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_end(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_endpackage(  pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_stack(      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_synchronize(pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/control/messages/ ...
PMATH_PRIVATE pmath_t builtin_assign_messagename(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_messagename(       pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_assign_messages(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_messages(       pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_criticalmessagetag(  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isvalidargumentcount(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_message(             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_messagecount(        pmath_expr_t expr); // in message.c
PMATH_PRIVATE pmath_t builtin_on_or_off(           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_try(                 pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/datetime/ ...
PMATH_PRIVATE pmath_t builtin_datelist(        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_pause(           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_timeconstrained( pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_timing(          pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/formating/ ...
PMATH_PRIVATE pmath_t builtin_baseform(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_makeboxes(        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_parenthesizeboxes(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_toboxes(          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_tostring(         pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_assign_makeboxes_or_format(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_assign_syntaxinformation(  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_syntaxinformation(         pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/gui/ ...
PMATH_PRIVATE pmath_t builtin_button( pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_clock(  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_refresh(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_internal_dynamicevaluate(             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_internal_getcurrentdynamicid(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_internal_dynamicevaluatemultiple(     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_internal_dynamicremove(               pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_internal_suppressdynamictrackingblock(pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/io/ ...
PMATH_PRIVATE pmath_t builtin_assign_currentdirectory(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_assign_environment(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_internal_stringpatternconvert(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_binaryread(                    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_binaryreadlist(                pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_binarywrite(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_characters(                    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_close(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_compress(                      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_compressstream(                pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_internal_getcurrentdirectory(  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_directoryname(                 pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_environment(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_filenames(                     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_filetype(                      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_find(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_findlist(                      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_fromcharactercode(             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_get(                           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_open(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_parentdirectory(               pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_print(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_read(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_readlist(                      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_resetdirectory(                pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_sectionprint(                  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_setdirectory(                  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_setstreamposition(             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_streamposition(                pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_stringcases(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_stringcount(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_stringdrop(                    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_stringexpression(              pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_stringmatch(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_stringposition(                pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_stringreplace(                 pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_stringsplit(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_stringtake(                    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_stringtostream(                pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_tocharactercode(               pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_tofilename(                    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_uncompress(                    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_uncompressstream(              pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_write(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_writestring(                   pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/language/ ...
PMATH_PRIVATE pmath_t builtin_developer_frompackedarray(   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_developer_getdebugmetadata(  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_developer_ispackedarray(     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_developer_setdebugmetadataat(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_developer_topackedarray(     pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_assign_namespace(    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_assign_namespacepath(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_isatom(           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_iscomplex(        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_iseven(           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isexactnumber(    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isfloat(          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isinexactnumber(  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isimaginary(      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isinteger(        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_ismachinenumber(  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isnumber(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isodd(            pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_ispos_or_isneg(   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isquotient(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isrational(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isreal(           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isstring(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_issymbol(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_makeexpression(   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_names(            pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_namespace(        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_remove(           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_stringtoboxes(    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_symbolname(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_toexpression(     pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_internal_parserealball(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_internal_writerealball(pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_private_getrefcount(pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/lists/ ...
PMATH_PRIVATE pmath_t builtin_part(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_assign_part(  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_replacepart(  pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_call_linearsolvefunction(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_call_list(               pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_apply(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_append(                       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_array(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_cases(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_complement(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_constantarray(                pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_depth(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_det(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_diagonalmatrix(               pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_dimensions(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_dot(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_drop(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_emit(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_extract(                      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_first(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_fixedpoint_and_fixedpointlist(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_flatten(                      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_fold(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_foldlist(                     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_gather(                       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_head(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_identitymatrix(               pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_inner(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_intersection(                 pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isarray(                      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isfreeof(                     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isordered(                    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_ismatrix(                     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isvector(                     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_join(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_keys(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_last(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_leafcount(                    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_length(                       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_lengthwhile(                  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_level(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_linearsolve(                  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_listconvolve(                 pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_lookup(                       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_ludecomposition(              pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_map(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_mapindexed(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_mapthread(                    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_max(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_mean(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_merge(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_min(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_minmax(                       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_most(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_nest(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_nestlist(                     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_nestwhile_and_nestwhilelist(  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_norm(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_operate(                      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_ordering(                     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_padleft_and_padright(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_partition(                    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_prepend(                      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_quantile(                     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_range(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_regather(                     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_rest(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_reverse(                      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_riffle(                       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_scan(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_select(                       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_sort(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_sortby(                       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_split(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_table(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_take(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_takewhile(                    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_thread(                       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_through(                      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_total(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_union(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_unitvector(                   pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/logic/ ...
PMATH_PRIVATE pmath_t builtin_and(                  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_boole(                pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_conditionalexpression(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_equal(                pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_greater(              pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_greaterequal(         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_identical(            pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_inequation(           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_less(                 pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_lessequal(            pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_not(                  pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_or(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_unequal(              pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_unidentical(          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_xor(                  pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_operate_conditionalexpression(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_operate_undefined(pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/manipulate/ ...
PMATH_PRIVATE pmath_t builtin_expand(   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_expandall(pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/number-theory/ ...
PMATH_PRIVATE pmath_t builtin_assign_isnumeric(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isnumeric(       pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_bitand(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_bitclear(                        pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_bitget(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_bitlength(                       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_bitnot(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_bitor(                           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_bitset(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_bitshiftleft(                    pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_bitshiftright(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_bitxor(                          pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_extendedgcd(                     pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_gcd(                             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_isprime(                         pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_jacobisymbol_and_kroneckersymbol(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_lcm(                             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_nextprime(                       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_randominteger(                   pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_randomreal(                      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_seedrandom(                      pmath_expr_t expr);
//} ============================================================================
//{ builtins from src/pmath-builtins/parallel/ ...
PMATH_PRIVATE pmath_t builtin_abort(             pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_getthreadid(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_newtask(           pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_parallelmap(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_parallelmapindexed(pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_parallelscan(      pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_paralleltry(       pmath_expr_t expr);
PMATH_PRIVATE pmath_t builtin_wait(              pmath_expr_t expr);

PMATH_PRIVATE pmath_t builtin_internal_threadidle(pmath_expr_t expr);
//} ============================================================================

//{ general purpose builtin functions ...

static pmath_t general_builtin_zeroargs(pmath_expr_t expr) {
  if(pmath_expr_length(expr) != 0)
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
  return expr;
}

static pmath_t general_builtin_onearg(pmath_expr_t expr) {
  if(pmath_expr_length(expr) != 1)
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
  return expr;
}

static pmath_t general_builtin_zeroonearg(pmath_expr_t expr) {
  if(pmath_expr_length(expr) > 1)
    pmath_message_argxxx(pmath_expr_length(expr), 0, 1);
  return expr;
}

static pmath_t general_builtin_zerotwoarg(pmath_expr_t expr) {
  if(pmath_expr_length(expr) > 2)
    pmath_message_argxxx(pmath_expr_length(expr), 0, 2);
  return expr;
}

static pmath_t general_builtin_nofront(pmath_expr_t expr) {
  pmath_message(PMATH_NULL, "nofront", 0);
  pmath_unref(expr);
  return pmath_ref(pmath_System_DollarFailed);
}

//} ============================================================================

PMATH_API
pmath_bool_t pmath_register_code(
  pmath_symbol_t         symbol,
  pmath_builtin_func_t   func,
  pmath_code_usage_t     usage
) {
  struct _pmath_symbol_rules_t *rules;
  
  if(pmath_is_null(symbol))
    return FALSE;
  
  rules = _pmath_symbol_get_rules(symbol, RULES_WRITEOPTIONS);
  if(!rules)
    return FALSE;
  
  switch(usage) {
    case PMATH_CODE_USAGE_EARLYCALL: pmath_atomic_write_release(&rules->early_call,  (intptr_t)func); break;
    case PMATH_CODE_USAGE_DOWNCALL:  pmath_atomic_write_release(&rules->down_call,   (intptr_t)func); break;
    case PMATH_CODE_USAGE_UPCALL:    pmath_atomic_write_release(&rules->up_call,     (intptr_t)func); break;
    case PMATH_CODE_USAGE_SUBCALL:   pmath_atomic_write_release(&rules->sub_call,    (intptr_t)func); break;
    default:
      return FALSE;
  }
  
  return TRUE;
}

PMATH_API
pmath_bool_t pmath_register_approx_code(
  pmath_symbol_t       symbol,
  pmath_approx_func_t  func
) {
  struct _pmath_symbol_rules_t *rules;
  
  if(pmath_is_null(symbol))
    return FALSE;
  
  rules = _pmath_symbol_get_rules(symbol, RULES_WRITEOPTIONS);
  if(!rules)
    return FALSE;
  
  pmath_atomic_write_release(&rules->approx_call, (intptr_t)func);
  return TRUE;
}

/*============================================================================*/

static pmath_bool_t init_builtin_security_doormen(void) {
#define CHECK(x)  if(!(x)) return FALSE
  CHECK( pmath_security_register_doorman(builtin_abs,                   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_approximate,           PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_arg,                   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_binomial,              PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_chop,                  PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_complex,               PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_conjugate,             PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_denominator,           PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_directedinfinity,      PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_exp,                   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_factorial,             PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_factorial2,            PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_fractionalpart,        PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_gamma,                 PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_im,                    PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_log,                   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_loggamma,              PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_mod,                   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_numerator,             PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_numerator_denominator, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_plus,                  PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_polygamma,             PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_power,                 PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_powermod,              PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_precision,             PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_product,               PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_quotient,              PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_re,                    PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_rescale,               PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_round_functions,       PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_setprecision,          PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_sign,                  PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_sqrt,                  PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_sum,                   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_times,                 PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );

  CHECK( pmath_security_register_doorman(builtin_approximate_e,                PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_approximate_eulergamma,       PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_approximate_machineprecision, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_approximate_pi,               PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_approximate_power,            PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );

  CHECK( pmath_security_register_doorman(builtin_internal_copysign,                   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_internal_nexttoward,                 PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_internal_realballfrommidpointradius, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_internal_realballbounds,             PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_internal_realballmidpointradius,     PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_internal_signbit,                    PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );

  CHECK( pmath_security_register_doorman(builtin_evaluationsequence,          PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_internal_tryevaluatesecured, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_function,                    PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_call_function,               PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_call_linearsolvefunction,    PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_call_list,                   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_trustedfunction,             PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_call_trustedfunction,        PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  
  // builtin_block?
  CHECK( pmath_security_register_doorman(builtin_local, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_with,   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );

  CHECK( pmath_security_register_doorman(builtin_if,        PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_do,        PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_for,       PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_piecewise, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_while,     PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_which,     PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );

  CHECK( pmath_security_register_doorman(builtin_baseform,          PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_makeboxes,         PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_parenthesizeboxes, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_toboxes,           PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_tostring,          PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  
  CHECK( pmath_security_register_doorman(builtin_developer_frompackedarray,    PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_developer_getdebugmetadata,   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_developer_ispackedarray,      PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_developer_setdebugmetadataat, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_developer_topackedarray,      PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
 
  CHECK( pmath_security_register_doorman(builtin_isatom,          PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_iscomplex,       PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_iseven,          PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_isexactnumber,   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_isfloat,         PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_isinexactnumber, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_isimaginary,     PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_isinteger,       PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_ismachinenumber, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_isnumber,        PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_isodd,           PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_ispos_or_isneg,  PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_isquotient,      PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_isrational,      PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_isreal,          PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_isstring,        PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_issymbol,        PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  
  CHECK( pmath_security_register_doorman(builtin_makeexpression, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_names,          PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_namespace,      PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_stringtoboxes,  PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_symbolname,     PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_toexpression,   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  
  CHECK( pmath_security_register_doorman(builtin_internal_parserealball, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_internal_writerealball, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  
  // builtin_assign_part ...?
  CHECK( pmath_security_register_doorman(builtin_part,        PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_replacepart, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  
  CHECK( pmath_security_register_doorman(builtin_replace,     PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_lookup,      PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  
  CHECK( pmath_security_register_doorman(builtin_internal_stringpatternconvert, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_stringcases,                   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_stringcount,                   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_stringdrop,                    PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_stringexpression,              PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_stringmatch,                   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_stringposition,                PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_stringreplace,                 PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_stringsplit,                   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_stringtake,                    PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  
  CHECK( pmath_security_register_doorman(builtin_apply,          PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_append,         PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_array,          PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_cases,          PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_complement,     PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_constantarray,  PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_depth,          PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_det,            PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_diagonalmatrix, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_dimensions,     PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_dot,            PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_drop,           PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  // builtin_emit ?
  CHECK( pmath_security_register_doorman(builtin_extract,                       PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_first,                         PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_fixedpoint_and_fixedpointlist, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_flatten,                       PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_fold,                          PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_foldlist,                      PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  // builtin_gather ?
  CHECK( pmath_security_register_doorman(builtin_head,                        PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_identitymatrix,              PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_inner,                       PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_intersection,                PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_isarray,                     PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_isfreeof,                    PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_isordered,                   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_ismatrix,                    PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_isvector,                    PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_join,                        PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_last,                        PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_leafcount,                   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_length,                      PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_lengthwhile,                 PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_level,                       PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_linearsolve,                 PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_listconvolve,                PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_ludecomposition,             PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_map,                         PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_mapindexed,                  PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_mapthread,                   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_mean,                        PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_merge,                       PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_min,                         PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_minmax,                      PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_most,                        PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_nest,                        PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_nestlist,                    PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_nestwhile_and_nestwhilelist, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_norm,                        PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_operate,                     PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_ordering,                    PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_padleft_and_padright,        PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_partition,                   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_prepend,                     PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_quantile,                    PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_range,                       PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_regather,                    PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_rest,                        PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_reverse,                     PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_riffle,                      PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_scan,                        PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_select,                      PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_sort,                        PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_sortby,                      PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_split,                       PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  //CHECK( pmath_security_register_doorman(builtin_table,                       PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_take,                        PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_takewhile,                   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_thread,                      PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_through,                     PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_total,                       PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_union,                       PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_unitvector,                  PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  
  CHECK( pmath_security_register_doorman(builtin_and,                   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_boole,                 PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_conditionalexpression, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_equal,                 PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_greater,               PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_greaterequal,          PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_identical,             PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_inequation,            PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_less,                  PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_lessequal,             PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_not,                   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_or,                    PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_unequal,               PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_unidentical,           PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_xor,                   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  
  CHECK( pmath_security_register_doorman(builtin_operate_conditionalexpression, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_operate_undefined,             PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  
  CHECK( pmath_security_register_doorman(builtin_expand,    PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_expandall, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  
  CHECK( pmath_security_register_doorman(builtin_isnumeric, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  
  CHECK( pmath_security_register_doorman(builtin_bitand,                           PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_bitclear,                         PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_bitget,                           PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_bitlength,                        PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_bitnot,                           PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_bitor,                            PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_bitset,                           PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_bitshiftleft,                     PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_bitshiftright,                    PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_bitxor,                           PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_extendedgcd,                      PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_gcd,                              PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_isprime,                          PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_jacobisymbol_and_kroneckersymbol, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_lcm,                              PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_nextprime,                        PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  
  CHECK( pmath_security_register_doorman(general_builtin_zeroargs,   PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(general_builtin_onearg,     PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(general_builtin_zeroonearg, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(general_builtin_zerotwoarg, PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(general_builtin_nofront,    PMATH_SECURITY_LEVEL_PURE_DETERMINISTIC_ALLOWED, NULL) );
  
  
  
  CHECK( pmath_security_register_doorman(builtin_randominteger, PMATH_SECURITY_LEVEL_NON_DESTRUCTIVE_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_randomreal,    PMATH_SECURITY_LEVEL_NON_DESTRUCTIVE_ALLOWED, NULL) );
  CHECK( pmath_security_register_doorman(builtin_seedrandom,    PMATH_SECURITY_LEVEL_NON_DESTRUCTIVE_ALLOWED, NULL) );

#undef CHECK
  return TRUE;
}

PMATH_PRIVATE pmath_bool_t _pmath_symbol_builtins_init(void) {
#define VERIFY(X)  do{ pmath_t tmp = (X); if(pmath_is_null(tmp)) goto FAIL; }while(0);

#define PMATH_DECLARE_SYMBOL(SYM, NAME, ATTRIB)    VERIFY( SYM = pmath_symbol_get(PMATH_C_STRING(NAME), TRUE) );
#  include "symbols.inc"
#undef PMATH_DECLARE_SYMBOL


  //{ binding C functions ...
#define BIND(sym, func, use)  if(!pmath_register_code((sym), (func), (use))) goto FAIL;
#define BIND_APPROX(sym, func) if(!pmath_register_approx_code((sym), (func))) goto FAIL;
#define BIND_EARLY(sym, func)  do{BIND((sym), (func), PMATH_CODE_USAGE_EARLYCALL) BIND_DOWN(sym, func)}while(0);
#define BIND_DOWN(sym, func)   BIND((sym), (func), PMATH_CODE_USAGE_DOWNCALL)
#define BIND_SUB(sym, func)    BIND((sym), (func), PMATH_CODE_USAGE_SUBCALL)
#define BIND_UP(sym, func)     BIND((sym), (func), PMATH_CODE_USAGE_UPCALL)
  
  BIND_EARLY(  pmath_System_Plus,                         builtin_plus)
  BIND_EARLY(  pmath_System_Power,                        builtin_power)
  BIND_EARLY(  pmath_System_Times,                        builtin_times)
  
  BIND_SUB(    pmath_System_Function,                     builtin_call_function)
  BIND_SUB(    pmath_System_IsHeld,                       builtin_call_isheld)
  BIND_SUB(    pmath_System_LinearSolveFunction,          builtin_call_linearsolvefunction)
  BIND_SUB(    pmath_System_List,                         builtin_call_list)
  BIND_SUB(    pmath_System_TrustedFunction,              builtin_call_trustedfunction)
  
  BIND_UP(     pmath_System_NRules,                       builtin_assign_symbol_rules)
  BIND_UP(     pmath_System_Attributes,                   builtin_assign_attributes)
  BIND_UP(     pmath_System_DollarCurrentDirectory,       builtin_assign_currentdirectory)
  BIND_UP(     pmath_System_DollarNamespace,              builtin_assign_namespace)
  BIND_UP(     pmath_System_Default,                      builtin_assign_default)
  BIND_UP(     pmath_System_DefaultRules,                 builtin_assign_symbol_rules)
  BIND_UP(     pmath_System_DownRules,                    builtin_assign_symbol_rules)
  BIND_UP(     pmath_System_Environment,                  builtin_assign_environment)
  BIND_UP(     pmath_System_Format,                       builtin_assign_makeboxes_or_format)
  BIND_UP(     pmath_System_FormatRules,                  builtin_assign_symbol_rules)
  BIND_UP(     pmath_Internal_DollarNamespaceStack,       builtin_assign_namespacepath)
  BIND_UP(     pmath_System_IsNumeric,                    builtin_assign_isnumeric)
  BIND_UP(     pmath_System_List,                         builtin_assign_list)
  BIND_UP(     pmath_System_MakeBoxes,                    builtin_assign_makeboxes_or_format)
  BIND_UP(     pmath_System_DollarMaxExtraPrecision,      builtin_assign_maxextraprecision)
  BIND_UP(     pmath_System_MessageName,                  builtin_assign_messagename)
  BIND_UP(     pmath_System_Messages,                     builtin_assign_messages)
  BIND_UP(     pmath_System_DollarNamespacePath,          builtin_assign_namespacepath)
  BIND_UP(     pmath_System_Options,                      builtin_assign_options)
  BIND_UP(     pmath_System_OwnRules,                     builtin_assign_ownrules)
  BIND_UP(     pmath_System_Part,                         builtin_assign_part)
  BIND_UP(     pmath_System_SetPrecision,                 builtin_assign_setprecision)
  BIND_UP(     pmath_System_SubRules,                     builtin_assign_symbol_rules)
  BIND_UP(     pmath_System_SyntaxInformation,            builtin_assign_syntaxinformation)
  BIND_UP(     pmath_System_UpRules,                      builtin_assign_symbol_rules)
  
  BIND_UP(     pmath_System_ConditionalExpression,        builtin_operate_conditionalexpression)
  BIND_UP(     pmath_System_Undefined,                    builtin_operate_undefined)
  
  BIND_DOWN(   pmath_System_Private_GetRefCount,          builtin_private_getrefcount)
  BIND_DOWN(   pmath_System_Private_IsValid,              builtin_private_isvalid)
  BIND_DOWN(   pmath_System_Private_SetValid,             builtin_private_setvalid)
  
  BIND_DOWN(   pmath_Internal_AbortMessage,                 builtin_internal_abortmessage)
  BIND_DOWN(   pmath_Internal_BlockUserDefinitions,         builtin_internal_blockuserdefinitions)
  BIND_DOWN(   pmath_Internal_CopySign,                     builtin_internal_copysign)
  BIND_DOWN(   pmath_Internal_CriticalMessageTag,           builtin_criticalmessagetag)
  BIND_DOWN(   pmath_Internal_DynamicEvaluate,              builtin_internal_dynamicevaluate)
  BIND_DOWN(   pmath_Internal_DynamicEvaluateMultiple,      builtin_internal_dynamicevaluatemultiple)
  BIND_DOWN(   pmath_Internal_DynamicRemove,                builtin_internal_dynamicremove)
  BIND_DOWN(   pmath_Internal_GetCurrentDirectory,          builtin_internal_getcurrentdirectory)
  BIND_DOWN(   pmath_Internal_GetCurrentDynamicID,          builtin_internal_getcurrentdynamicid)
  BIND_DOWN(   pmath_Internal_GetThreadID,                  builtin_getthreadid)
  BIND_DOWN(   pmath_Internal_MakeTrustedFunction,          builtin_internal_maketrustedfunction)
  BIND_DOWN(   pmath_Internal_NextToward,                   builtin_internal_nexttoward)
  BIND_DOWN(   pmath_Internal_ParseRealBall,                builtin_internal_parserealball)
  BIND_DOWN(   pmath_Internal_RealBallBounds,               builtin_internal_realballbounds)
  BIND_DOWN(   pmath_Internal_RealBallFromMidpointRadius,   builtin_internal_realballfrommidpointradius)
  BIND_DOWN(   pmath_Internal_RealBallMidpointRadius,       builtin_internal_realballmidpointradius)
  BIND_DOWN(   pmath_Internal_SignBit,                      builtin_internal_signbit)
  BIND_DOWN(   pmath_Internal_StringPatternConvert,         builtin_internal_stringpatternconvert)
  BIND_DOWN(   pmath_Internal_SuppressDynamicTrackingBlock, builtin_internal_suppressdynamictrackingblock)
  BIND_DOWN(   pmath_Internal_ThreadIdle,                   builtin_internal_threadidle)
  BIND_DOWN(   pmath_Internal_TryEvaluateSecured,           builtin_internal_tryevaluatesecured)
  BIND_DOWN(   pmath_Internal_WriteRealBall,                builtin_internal_writerealball)
  
  BIND_DOWN(   pmath_Developer_FromPackedArray,           builtin_developer_frompackedarray)
  BIND_DOWN(   pmath_Developer_GetDebugMetadata,          builtin_developer_getdebugmetadata)
  BIND_DOWN(   pmath_Developer_HasAssignedRules,          builtin_developer_hasassignedrules)
  BIND_DOWN(   pmath_Developer_HasBuiltinCode,            builtin_developer_hasbuiltincode)
  BIND_DOWN(   pmath_Developer_IsPackedArray,             builtin_developer_ispackedarray)
  BIND_DOWN(   pmath_Developer_SetDebugMetadataAt,        builtin_developer_setdebugmetadataat)
  BIND_DOWN(   pmath_Developer_ToPackedArray,             builtin_developer_topackedarray)
  
  BIND_DOWN(   pmath_System_Abort,                        builtin_abort)
  BIND_DOWN(   pmath_System_Abs,                          builtin_abs)
  BIND_DOWN(   pmath_System_And,                          builtin_and)
  BIND_DOWN(   pmath_System_Apply,                        builtin_apply)
  BIND_DOWN(   pmath_System_Append,                       builtin_append)
  BIND_DOWN(   pmath_System_Arg,                          builtin_arg)
  BIND_DOWN(   pmath_System_Array,                        builtin_array)
  BIND_DOWN(   pmath_System_Assign,                       builtin_assign)
  BIND_DOWN(   pmath_System_AssignDelayed,                builtin_assign)
  BIND_DOWN(   pmath_System_AssignWith,                   builtin_assignwith)
  BIND_DOWN(   pmath_System_Attributes,                   builtin_attributes)
  BIND_DOWN(   pmath_System_BaseForm,                     builtin_baseform)
  BIND_DOWN(   pmath_System_Begin,                        builtin_begin)
  BIND_DOWN(   pmath_System_BeginPackage,                 builtin_beginpackage)
  BIND_DOWN(   pmath_System_BinaryRead,                   builtin_binaryread)
  BIND_DOWN(   pmath_System_BinaryReadList,               builtin_binaryreadlist)
  BIND_DOWN(   pmath_System_BinaryWrite,                  builtin_binarywrite)
  BIND_DOWN(   pmath_System_Binomial,                     builtin_binomial)
  BIND_DOWN(   pmath_System_BitAnd,                       builtin_bitand)
  BIND_DOWN(   pmath_System_BitClear,                     builtin_bitclear)
  BIND_DOWN(   pmath_System_BitGet,                       builtin_bitget)
  BIND_DOWN(   pmath_System_BitLength,                    builtin_bitlength)
  BIND_DOWN(   pmath_System_BitNot,                       builtin_bitnot)
  BIND_DOWN(   pmath_System_BitOr,                        builtin_bitor)
  BIND_DOWN(   pmath_System_BitSet,                       builtin_bitset)
  BIND_DOWN(   pmath_System_BitShiftLeft,                 builtin_bitshiftleft)
  BIND_DOWN(   pmath_System_BitShiftRight,                builtin_bitshiftright)
  BIND_DOWN(   pmath_System_BitXor,                       builtin_bitxor)
  BIND_DOWN(   pmath_System_Block,                        builtin_block)
  BIND_DOWN(   pmath_System_Boole,                        builtin_boole)
  BIND_DOWN(   pmath_System_Break,                        general_builtin_zeroonearg)
  BIND_DOWN(   pmath_System_Button,                       builtin_button)
  BIND_DOWN(   pmath_System_ByteCount,                    builtin_bytecount)
  BIND_DOWN(   pmath_System_Cases,                        builtin_cases)
  BIND_DOWN(   pmath_System_Catch,                        builtin_catch)
  BIND_DOWN(   pmath_System_Ceiling,                      builtin_round_functions)
  BIND_DOWN(   pmath_System_Characters,                   builtin_characters)
  BIND_DOWN(   pmath_System_Chop,                         builtin_chop)
  BIND_DOWN(   pmath_System_Clear,                        builtin_clear_or_clearall)
  BIND_DOWN(   pmath_System_ClearAll,                     builtin_clear_or_clearall)
  BIND_DOWN(   pmath_System_ClearAttributes,              builtin_clearattributes)
  BIND_DOWN(   pmath_System_Clock,                        builtin_clock)
  BIND_DOWN(   pmath_System_Close,                        builtin_close)
  BIND_DOWN(   pmath_System_Complement,                   builtin_complement)
  BIND_DOWN(   pmath_System_Complex,                      builtin_complex)
  BIND_DOWN(   pmath_System_Compress,                     builtin_compress)
  BIND_DOWN(   pmath_System_CompressStream,               builtin_compressstream)
  BIND_DOWN(   pmath_System_ConditionalExpression,        builtin_conditionalexpression)
  BIND_DOWN(   pmath_System_Conjugate,                    builtin_conjugate)
  BIND_DOWN(   pmath_System_ConstantArray,                builtin_constantarray)
  BIND_DOWN(   pmath_System_Continue,                     general_builtin_zeroonearg)
  BIND_DOWN(   pmath_System_Count,                        builtin_count)
  BIND_DOWN(   pmath_System_CreateDocument,               general_builtin_nofront)
  BIND_DOWN(   pmath_System_CurrentValue,                 general_builtin_nofront)
  BIND_DOWN(   pmath_System_DateList,                     builtin_datelist)
  BIND_DOWN(   pmath_System_Decrement,                    builtin_dec_or_inc_or_postdec_or_postinc)
  BIND_DOWN(   pmath_System_Default,                      builtin_default)
  BIND_DOWN(   pmath_System_DefaultRules,                 builtin_symbol_rules)
  BIND_DOWN(   pmath_System_Denominator,                  builtin_denominator)
  BIND_DOWN(   pmath_System_Depth,                        builtin_depth)
  BIND_DOWN(   pmath_System_Det,                          builtin_det)
  BIND_DOWN(   pmath_System_DiagonalMatrix,               builtin_diagonalmatrix)
  BIND_DOWN(   pmath_System_Dialog,                       general_builtin_nofront)
  BIND_DOWN(   pmath_System_Dimensions,                   builtin_dimensions)
  BIND_DOWN(   pmath_System_DirectedInfinity,             builtin_directedinfinity)
  BIND_DOWN(   pmath_System_DirectoryName,                builtin_directoryname)
  BIND_DOWN(   pmath_System_DivideBy,                     builtin_divideby_or_timesby)
  BIND_DOWN(   pmath_System_Do,                           builtin_do)
  BIND_DOWN(   pmath_System_DocumentApply,                general_builtin_nofront);
  BIND_DOWN(   pmath_System_DocumentDelete,               general_builtin_nofront);
  BIND_DOWN(   pmath_System_DocumentGet,                  general_builtin_nofront);
  BIND_DOWN(   pmath_System_DocumentRead,                 general_builtin_nofront);
  BIND_DOWN(   pmath_System_DocumentWrite,                general_builtin_nofront);
  BIND_DOWN(   pmath_System_Documents,                    general_builtin_nofront);
  BIND_DOWN(   pmath_System_DocumentSave,                 general_builtin_nofront);
  BIND_DOWN(   pmath_System_Dot,                          builtin_dot)
  BIND_DOWN(   pmath_System_DownRules,                    builtin_symbol_rules)
  BIND_DOWN(   pmath_System_Drop,                         builtin_drop)
  BIND_DOWN(   pmath_System_Emit,                         builtin_emit)
  BIND_DOWN(   pmath_System_End,                          builtin_end)
  BIND_DOWN(   pmath_System_EndPackage,                   builtin_endpackage)
  BIND_DOWN(   pmath_System_Environment,                  builtin_environment)
  BIND_DOWN(   pmath_System_Equal,                        builtin_equal)
  BIND_DOWN(   pmath_System_Evaluate,                     builtin_evaluate)
  BIND_DOWN(   pmath_System_EvaluateDelayed,              builtin_evaluatedelayed)
  BIND_DOWN(   pmath_System_EvaluationDocument,           general_builtin_nofront)
  BIND_DOWN(   pmath_System_EvaluationSequence,           builtin_evaluationsequence)
  BIND_DOWN(   pmath_System_Exp,                          builtin_exp)
  BIND_DOWN(   pmath_System_Expand,                       builtin_expand)
  BIND_DOWN(   pmath_System_ExpandAll,                    builtin_expandall)
  BIND_DOWN(   pmath_System_ExtendedGCD,                  builtin_extendedgcd)
  BIND_DOWN(   pmath_System_Extract,                      builtin_extract)
  BIND_DOWN(   pmath_System_Factorial,                    builtin_factorial)
  BIND_DOWN(   pmath_System_Factorial2,                   builtin_factorial2)
  BIND_DOWN(   pmath_System_FileNames,                    builtin_filenames)
  BIND_DOWN(   pmath_System_FileType,                     builtin_filetype)
  BIND_DOWN(   pmath_System_FilterRules,                  builtin_filterrules)
  BIND_DOWN(   pmath_System_Finally,                      builtin_finally)
  BIND_DOWN(   pmath_System_Find,                         builtin_find)
  BIND_DOWN(   pmath_System_FindList,                     builtin_findlist)
  BIND_DOWN(   pmath_System_First,                        builtin_first)
  BIND_DOWN(   pmath_System_FixedPoint,                   builtin_fixedpoint_and_fixedpointlist)
  BIND_DOWN(   pmath_System_FixedPointList,               builtin_fixedpoint_and_fixedpointlist)
  BIND_DOWN(   pmath_System_Flatten,                      builtin_flatten)
  BIND_DOWN(   pmath_System_Floor,                        builtin_round_functions)
  BIND_DOWN(   pmath_System_Fold,                         builtin_fold)
  BIND_DOWN(   pmath_System_FoldList,                     builtin_foldlist)
  BIND_DOWN(   pmath_System_For,                          builtin_for)
  BIND_DOWN(   pmath_System_FormatRules,                  builtin_symbol_rules)
  BIND_DOWN(   pmath_System_FractionalPart,               builtin_fractionalpart)
  BIND_DOWN(   pmath_System_FromCharacterCode,            builtin_fromcharactercode)
  BIND_DOWN(   pmath_System_FrontEndTokenExecute,         general_builtin_nofront)
  BIND_DOWN(   pmath_System_Function,                     builtin_function)
  BIND_DOWN(   pmath_System_Gamma,                        builtin_gamma)
  BIND_DOWN(   pmath_System_Gather,                       builtin_gather)
  BIND_DOWN(   pmath_System_GCD,                          builtin_gcd)
  BIND_DOWN(   pmath_System_Get,                          builtin_get)
  BIND_DOWN(   pmath_System_Goto,                         general_builtin_onearg)
  BIND_DOWN(   pmath_System_Greater,                      builtin_greater)
  BIND_DOWN(   pmath_System_GreaterEqual,                 builtin_greaterequal)
  BIND_DOWN(   pmath_System_Hash,                         builtin_hash)
  BIND_DOWN(   pmath_System_Head,                         builtin_head)
  BIND_DOWN(   pmath_System_DollarHistory,                builtin_history)
  BIND_DOWN(   pmath_System_Identical,                    builtin_identical)
  BIND_DOWN(   pmath_System_Identity,                     builtin_evaluate)
  BIND_DOWN(   pmath_System_IdentityMatrix,               builtin_identitymatrix)
  BIND_DOWN(   pmath_System_If,                           builtin_if)
  BIND_DOWN(   pmath_System_Im,                           builtin_im)
  BIND_DOWN(   pmath_System_Increment,                    builtin_dec_or_inc_or_postdec_or_postinc)
  BIND_DOWN(   pmath_System_Inequation,                   builtin_inequation)
  BIND_DOWN(   pmath_System_Inner,                        builtin_inner)
  BIND_DOWN(   pmath_System_IntegerPart,                  builtin_round_functions)
  BIND_DOWN(   pmath_System_Interrupt,                    general_builtin_nofront)
  BIND_DOWN(   pmath_System_Intersection,                 builtin_intersection)
  BIND_DOWN(   pmath_System_IsArray,                      builtin_isarray)
  BIND_DOWN(   pmath_System_IsAtom,                       builtin_isatom)
  BIND_DOWN(   pmath_System_IsComplex,                    builtin_iscomplex)
  BIND_DOWN(   pmath_System_IsEven,                       builtin_iseven)
  BIND_DOWN(   pmath_System_IsExactNumber,                builtin_isexactnumber)
  BIND_DOWN(   pmath_System_IsFloat,                      builtin_isfloat)
  BIND_DOWN(   pmath_System_IsFreeOf,                     builtin_isfreeof)
  BIND_DOWN(   pmath_System_IsHeld,                       builtin_isheld)
  BIND_DOWN(   pmath_System_IsImaginary,                  builtin_isimaginary)
  BIND_DOWN(   pmath_System_IsInexactNumber,              builtin_isinexactnumber)
  BIND_DOWN(   pmath_System_IsInteger,                    builtin_isinteger)
  BIND_DOWN(   pmath_System_IsMachineNumber,              builtin_ismachinenumber)
  BIND_DOWN(   pmath_System_IsMatrix,                     builtin_ismatrix)
  BIND_DOWN(   pmath_System_IsNegative,                   builtin_ispos_or_isneg)
  BIND_DOWN(   pmath_System_IsNonNegative,                builtin_ispos_or_isneg)
  BIND_DOWN(   pmath_System_IsNonPositive,                builtin_ispos_or_isneg)
  BIND_DOWN(   pmath_System_IsNumber,                     builtin_isnumber)
  BIND_DOWN(   pmath_System_IsNumeric,                    builtin_isnumeric)
  BIND_DOWN(   pmath_System_IsOdd,                        builtin_isodd)
  BIND_DOWN(   pmath_System_IsOption,                     builtin_isoption)
  BIND_DOWN(   pmath_System_IsOrdered,                    builtin_isordered)
  BIND_DOWN(   pmath_System_IsPositive,                   builtin_ispos_or_isneg)
  BIND_DOWN(   pmath_System_IsPrime,                      builtin_isprime)
  BIND_DOWN(   pmath_System_IsQuotient,                   builtin_isquotient)
  BIND_DOWN(   pmath_System_IsRational,                   builtin_isrational)
  BIND_DOWN(   pmath_System_IsReal,                       builtin_isreal)
  BIND_DOWN(   pmath_System_IsString,                     builtin_isstring)
  BIND_DOWN(   pmath_System_IsSymbol,                     builtin_issymbol)
  BIND_DOWN(   pmath_System_IsValidArgumentCount,         builtin_isvalidargumentcount)
  BIND_DOWN(   pmath_System_IsVector,                     builtin_isvector)
  BIND_DOWN(   pmath_System_JacobiSymbol,                 builtin_jacobisymbol_and_kroneckersymbol)
  BIND_DOWN(   pmath_System_Join,                         builtin_join)
  BIND_DOWN(   pmath_System_Keys,                         builtin_keys)
  BIND_DOWN(   pmath_System_KroneckerSymbol,              builtin_jacobisymbol_and_kroneckersymbol)
  BIND_DOWN(   pmath_System_Label,                        general_builtin_onearg)
  BIND_DOWN(   pmath_System_Last,                         builtin_last)
  BIND_DOWN(   pmath_System_LCM,                          builtin_lcm)
  BIND_DOWN(   pmath_System_LeafCount,                    builtin_leafcount)
  BIND_DOWN(   pmath_System_Length,                       builtin_length)
  BIND_DOWN(   pmath_System_LengthWhile,                  builtin_lengthwhile)
  BIND_DOWN(   pmath_System_Less,                         builtin_less)
  BIND_DOWN(   pmath_System_LessEqual,                    builtin_lessequal)
  BIND_DOWN(   pmath_System_Level,                        builtin_level)
  BIND_DOWN(   pmath_System_LinearSolve,                  builtin_linearsolve)
  BIND_DOWN(   pmath_System_ListConvolve,                 builtin_listconvolve)
  BIND_DOWN(   pmath_System_LoadLibrary,                  builtin_loadlibrary)
  BIND_DOWN(   pmath_System_Local,                        builtin_local)
  BIND_DOWN(   pmath_System_Log,                          builtin_log)
  BIND_DOWN(   pmath_System_LogGamma,                     builtin_loggamma)
  BIND_DOWN(   pmath_System_Lookup,                       builtin_lookup)
  BIND_DOWN(   pmath_System_LUDecomposition,              builtin_ludecomposition)
  BIND_DOWN(   pmath_System_Map,                          builtin_map)
  BIND_DOWN(   pmath_System_MapIndexed,                   builtin_mapindexed)
  BIND_DOWN(   pmath_System_MapThread,                    builtin_mapthread)
  BIND_DOWN(   pmath_System_MakeBoxes,                    builtin_makeboxes)
  BIND_DOWN(   pmath_System_MakeExpression,               builtin_makeexpression)
  BIND_DOWN(   pmath_System_Match,                        builtin_match)
  BIND_DOWN(   pmath_System_Max,                          builtin_max)
  BIND_DOWN(   pmath_System_Mean,                         builtin_mean)
  BIND_DOWN(   pmath_System_MemoryUsage,                  builtin_memoryusage)
  BIND_DOWN(   pmath_System_Merge,                        builtin_merge)
  BIND_DOWN(   pmath_System_Message,                      builtin_message)
  BIND_DOWN(   pmath_System_DollarMessageCount,           builtin_messagecount)
  BIND_DOWN(   pmath_System_MessageName,                  builtin_messagename)
  BIND_DOWN(   pmath_System_Messages,                     builtin_messages)
  BIND_DOWN(   pmath_System_Min,                          builtin_min)
  BIND_DOWN(   pmath_System_MinMax,                       builtin_minmax)
  BIND_DOWN(   pmath_System_Mod,                          builtin_mod)
  BIND_DOWN(   pmath_System_Most,                         builtin_most)
  BIND_DOWN(   pmath_System_N,                            builtin_approximate)
  BIND_DOWN(   pmath_System_Names,                        builtin_names)
  BIND_DOWN(   pmath_System_Namespace,                    builtin_namespace)
  BIND_DOWN(   pmath_System_Nest,                         builtin_nest)
  BIND_DOWN(   pmath_System_NestList,                     builtin_nestlist)
  BIND_DOWN(   pmath_System_NestWhile,                    builtin_nestwhile_and_nestwhilelist)
  BIND_DOWN(   pmath_System_NestWhileList,                builtin_nestwhile_and_nestwhilelist)
  BIND_DOWN(   pmath_System_NewTask,                      builtin_newtask)
  BIND_DOWN(   pmath_System_NextPrime,                    builtin_nextprime)
  BIND_DOWN(   pmath_System_Not,                          builtin_not)
  BIND_DOWN(   pmath_System_Norm,                         builtin_norm)
  BIND_DOWN(   pmath_System_NRules,                       builtin_symbol_rules)
  BIND_DOWN(   pmath_System_Numerator,                    builtin_numerator)
  BIND_DOWN(   pmath_System_NumeratorDenominator,         builtin_numerator_denominator)
  BIND_DOWN(   pmath_System_Off,                          builtin_on_or_off)
  BIND_DOWN(   pmath_System_On,                           builtin_on_or_off)
  BIND_DOWN(   pmath_System_Or,                           builtin_or)
  BIND_DOWN(   pmath_System_Ordering,                     builtin_ordering)
  BIND_DOWN(   pmath_System_OpenAppend,                   builtin_open)
  BIND_DOWN(   pmath_System_OpenRead,                     builtin_open)
  BIND_DOWN(   pmath_System_OpenWrite,                    builtin_open)
  BIND_DOWN(   pmath_System_Operate,                      builtin_operate)
  BIND_DOWN(   pmath_System_Options,                      builtin_options)
  BIND_DOWN(   pmath_System_OptionsPattern,               general_builtin_zeroonearg)
  BIND_DOWN(   pmath_System_OptionValue,                  builtin_optionvalue)
  BIND_DOWN(   pmath_System_Overflow,                     general_builtin_zeroargs)
  BIND_DOWN(   pmath_System_OwnRules,                     builtin_ownrules)
  BIND_DOWN(   pmath_System_PadLeft,                      builtin_padleft_and_padright)
  BIND_DOWN(   pmath_System_PadRight,                     builtin_padleft_and_padright)
  BIND_DOWN(   pmath_System_ParallelMap,                  builtin_parallelmap)
  BIND_DOWN(   pmath_System_ParallelMapIndexed,           builtin_parallelmapindexed)
  BIND_DOWN(   pmath_System_ParallelScan,                 builtin_parallelscan)
  BIND_DOWN(   pmath_System_ParallelTry,                  builtin_paralleltry)
  BIND_DOWN(   pmath_System_ParentDirectory,              builtin_parentdirectory)
  BIND_DOWN(   pmath_System_ParenthesizeBoxes,            builtin_parenthesizeboxes)
  BIND_DOWN(   pmath_System_Part,                         builtin_part)
  BIND_DOWN(   pmath_System_Partition,                    builtin_partition)
  BIND_DOWN(   pmath_System_Pause,                        builtin_pause)
  BIND_DOWN(   pmath_System_Piecewise,                    builtin_piecewise)
  BIND_DOWN(   pmath_System_PolyGamma,                    builtin_polygamma)
  BIND_DOWN(   pmath_System_Position,                     builtin_position)
  BIND_DOWN(   pmath_System_PostDecrement,                builtin_dec_or_inc_or_postdec_or_postinc)
  BIND_DOWN(   pmath_System_PostIncrement,                builtin_dec_or_inc_or_postdec_or_postinc)
  BIND_DOWN(   pmath_System_PowerMod,                     builtin_powermod)
  BIND_DOWN(   pmath_System_Precision,                    builtin_precision)
  BIND_DOWN(   pmath_System_Prepend,                      builtin_prepend)
  BIND_DOWN(   pmath_System_Print,                        builtin_print)
  BIND_DOWN(   pmath_System_Product,                      builtin_product)
  BIND_DOWN(   pmath_System_Protect,                      builtin_protect_or_unprotect)
  BIND_DOWN(   pmath_System_Quantile,                     builtin_quantile)
  BIND_DOWN(   pmath_System_Quotient,                     builtin_quotient)
  BIND_DOWN(   pmath_System_RandomInteger,                builtin_randominteger)
  BIND_DOWN(   pmath_System_RandomReal,                   builtin_randomreal)
  BIND_DOWN(   pmath_System_Range,                        builtin_range)
  BIND_DOWN(   pmath_System_Re,                           builtin_re)
  BIND_DOWN(   pmath_System_Read,                         builtin_read)
  BIND_DOWN(   pmath_System_ReadList,                     builtin_readlist)
  BIND_DOWN(   pmath_System_Refresh,                      builtin_refresh)
  BIND_DOWN(   pmath_System_ReGather,                     builtin_regather)
  BIND_DOWN(   pmath_System_ReleaseHold,                  builtin_releasehold)
  BIND_DOWN(   pmath_System_Remove,                       builtin_remove)
  BIND_DOWN(   pmath_System_Replace,                      builtin_replace)
  BIND_DOWN(   pmath_System_ReplaceList,                  builtin_replacelist)
  BIND_DOWN(   pmath_System_ReplacePart,                  builtin_replacepart)
  BIND_DOWN(   pmath_System_ReplaceRepeated,              builtin_replace)
  BIND_DOWN(   pmath_System_Rescale,                      builtin_rescale)
  BIND_DOWN(   pmath_System_ResetDirectory,               builtin_resetdirectory)
  BIND_DOWN(   pmath_System_Rest,                         builtin_rest)
  BIND_DOWN(   pmath_System_Return,                       general_builtin_zerotwoarg)
  BIND_DOWN(   pmath_System_Reverse,                      builtin_reverse)
  BIND_DOWN(   pmath_System_Riffle,                       builtin_riffle)
  BIND_DOWN(   pmath_System_Round,                        builtin_round_functions)
  BIND_DOWN(   pmath_System_Scan,                         builtin_scan)
  BIND_DOWN(   pmath_System_SectionPrint,                 builtin_sectionprint)
  BIND_DOWN(   pmath_System_SeedRandom,                   builtin_seedrandom)
  BIND_DOWN(   pmath_System_Select,                       builtin_select)
  BIND_DOWN(   pmath_System_SelectedDocument,             general_builtin_nofront);
  BIND_DOWN(   pmath_System_SetAttributes,                builtin_setattributes)
  BIND_DOWN(   pmath_System_SetDirectory,                 builtin_setdirectory)
  BIND_DOWN(   pmath_System_SetOptions,                   builtin_setoptions)
  BIND_DOWN(   pmath_System_SetPrecision,                 builtin_setprecision)
  BIND_DOWN(   pmath_System_SetStreamPosition,            builtin_setstreamposition)
  BIND_DOWN(   pmath_System_ShowDefinition,               builtin_showdefinition)
  BIND_DOWN(   pmath_System_Sign,                         builtin_sign)
  BIND_DOWN(   pmath_System_SingleMatch,                  general_builtin_zeroonearg)
  BIND_DOWN(   pmath_System_Sort,                         builtin_sort)
  BIND_DOWN(   pmath_System_SortBy,                       builtin_sortby)
  BIND_DOWN(   pmath_System_Split,                        builtin_split)
  BIND_DOWN(   pmath_System_Sqrt,                         builtin_sqrt)
  BIND_DOWN(   pmath_System_Stack,                        builtin_stack)
  BIND_DOWN(   pmath_System_StreamPosition,               builtin_streamposition)
  BIND_DOWN(   pmath_System_StringCases,                  builtin_stringcases)
  BIND_DOWN(   pmath_System_StringCount,                  builtin_stringcount)
  BIND_DOWN(   pmath_System_StringDrop,                   builtin_stringdrop)
  BIND_DOWN(   pmath_System_StringExpression,             builtin_stringexpression)
  BIND_DOWN(   pmath_System_StringMatch,                  builtin_stringmatch)
  BIND_DOWN(   pmath_System_StringPosition,               builtin_stringposition)
  BIND_DOWN(   pmath_System_StringReplace,                builtin_stringreplace)
  BIND_DOWN(   pmath_System_StringSplit,                  builtin_stringsplit)
  BIND_DOWN(   pmath_System_StringTake,                   builtin_stringtake)
  BIND_DOWN(   pmath_System_StringToBoxes,                builtin_stringtoboxes)
  BIND_DOWN(   pmath_System_StringToStream,               builtin_stringtostream)
  BIND_DOWN(   pmath_System_Sum,                          builtin_sum)
  BIND_DOWN(   pmath_System_SubRules,                     builtin_symbol_rules)
  BIND_DOWN(   pmath_System_SymbolName,                   builtin_symbolname)
  BIND_DOWN(   pmath_System_Synchronize,                  builtin_synchronize)
  BIND_DOWN(   pmath_System_SyntaxInformation,            builtin_syntaxinformation)
  BIND_DOWN(   pmath_System_SystemOpen,                   general_builtin_nofront)
  BIND_DOWN(   pmath_System_Table,                        builtin_table)
  BIND_DOWN(   pmath_System_TagAssign,                    builtin_tagassign)
  BIND_DOWN(   pmath_System_TagAssignDelayed,             builtin_tagassign)
  BIND_DOWN(   pmath_System_TagUnassign,                  builtin_tagunassign)
  BIND_DOWN(   pmath_System_Take,                         builtin_take)
  BIND_DOWN(   pmath_System_TakeWhile,                    builtin_takewhile)
  BIND_DOWN(   pmath_System_Thread,                       builtin_thread)
  BIND_DOWN(   pmath_System_Through,                      builtin_through)
  BIND_DOWN(   pmath_System_Throw,                        builtin_throw)
  BIND_DOWN(   pmath_System_TimeConstrained,              builtin_timeconstrained)
  BIND_DOWN(   pmath_System_Timing,                       builtin_timing)
  BIND_DOWN(   pmath_System_TimesBy,                      builtin_divideby_or_timesby)
  BIND_DOWN(   pmath_System_ToBoxes,                      builtin_toboxes)
  BIND_DOWN(   pmath_System_ToCharacterCode,              builtin_tocharactercode)
  BIND_DOWN(   pmath_System_ToExpression,                 builtin_toexpression)
  BIND_DOWN(   pmath_System_ToFileName,                   builtin_tofilename)
  BIND_DOWN(   pmath_System_ToString,                     builtin_tostring)
  BIND_DOWN(   pmath_System_Total,                        builtin_total)
  BIND_DOWN(   pmath_System_TrustedFunction,              builtin_trustedfunction)
  BIND_DOWN(   pmath_System_Try,                          builtin_try)
  BIND_DOWN(   pmath_System_Unassign,                     builtin_unassign)
  BIND_DOWN(   pmath_System_Uncompress,                   builtin_uncompress)
  BIND_DOWN(   pmath_System_UncompressStream,             builtin_uncompressstream)
  BIND_DOWN(   pmath_System_Underflow,                    general_builtin_zeroargs)
  BIND_DOWN(   pmath_System_Unequal,                      builtin_unequal)
  BIND_DOWN(   pmath_System_Unidentical,                  builtin_unidentical)
  BIND_DOWN(   pmath_System_Union,                        builtin_union)
  BIND_DOWN(   pmath_System_UnitVector,                   builtin_unitvector)
  BIND_DOWN(   pmath_System_Unprotect,                    builtin_protect_or_unprotect)
  BIND_DOWN(   pmath_System_Update,                       builtin_update)
  BIND_DOWN(   pmath_System_UpRules,                      builtin_symbol_rules)
  BIND_DOWN(   pmath_System_Wait,                         builtin_wait)
  BIND_DOWN(   pmath_System_Which,                        builtin_which)
  BIND_DOWN(   pmath_System_While,                        builtin_while)
  BIND_DOWN(   pmath_System_With,                         builtin_with)
  BIND_DOWN(   pmath_System_Write,                        builtin_write)
  BIND_DOWN(   pmath_System_WriteString,                  builtin_writestring)
  BIND_DOWN(   pmath_System_Xor,                          builtin_xor)
  
  BIND_APPROX( pmath_System_ExponentialE,                 builtin_approximate_e);
  BIND_APPROX( pmath_System_EulerGamma,                   builtin_approximate_eulergamma);
  BIND_APPROX( pmath_System_MachinePrecision,             builtin_approximate_machineprecision);
  BIND_APPROX( pmath_System_Pi,                           builtin_approximate_pi);
  BIND_APPROX( pmath_System_Power,                        builtin_approximate_power);
  
#undef BIND
#undef BIND_APPROX
#undef BIND_EARLY
#undef BIND_DOWN
#undef BIND_SUB
#undef BIND_UP
  //} ... binding C functions
  
  //{ setting attributes (except the Protected attribute) ...
#define SET_ATTRIB(sym,attrib) if((attrib) & ~PMATH_SYMBOL_ATTRIBUTE_PROTECTED ) do { pmath_symbol_set_attributes((sym), (attrib) & ~PMATH_SYMBOL_ATTRIBUTE_PROTECTED); } while(0)

#define PMATH_DECLARE_SYMBOL( SYM, NAME, ATTRIB )  SET_ATTRIB( SYM, ATTRIB );
#  include "symbols.inc"
#undef PMATH_DECLARE_SYMBOL

#undef SET_ATTRIB
  //} ... setting attributes (except Protected attribute)
  
  if(!init_builtin_security_doormen()) goto FAIL;
  
  return TRUE;
  
FAIL:
#  define PMATH_DECLARE_SYMBOL(SYM, NAME, ATTRIB)    pmath_unref(SYM); SYM = PMATH_NULL;
#    include "symbols.inc"
#  undef PMATH_DECLARE_SYMBOL

  return FALSE;
}

PMATH_PRIVATE void _pmath_symbol_builtins_protect_all(void) {
#  define PMATH_DECLARE_SYMBOL(SYM, NAME, ATTRIB)    if((ATTRIB) & PMATH_SYMBOL_ATTRIBUTE_PROTECTED)  pmath_symbol_set_attributes((SYM), pmath_symbol_get_attributes((SYM)) | PMATH_SYMBOL_ATTRIBUTE_PROTECTED);
#    include "symbols.inc"
#  undef PMATH_DECLARE_SYMBOL


#define UNPROTECT(sym) pmath_symbol_set_attributes(sym, pmath_symbol_get_attributes(sym) & ~PMATH_SYMBOL_ATTRIBUTE_PROTECTED)
  
  UNPROTECT( pmath_Internal_CriticalMessageTag);
  UNPROTECT( pmath_System_Subscript);
  UNPROTECT( pmath_System_Subsuperscript);
  UNPROTECT( pmath_System_Superscript);
  
#undef UNPROTECT
}

PMATH_PRIVATE void _pmath_symbol_builtins_done(void) {
#  define PMATH_DECLARE_SYMBOL(SYM, NAME, ATTRIB)    pmath_unref(SYM); SYM = PMATH_NULL;
#    include "symbols.inc"
#  undef PMATH_DECLARE_SYMBOL
}
