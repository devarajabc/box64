#if !(defined(GO) && defined(GOM) && defined(GO2) && defined(DATA))
#error meh!
#endif


//GO(gcry_calloc, 
//GO(gcry_calloc_secure, 
GO(gcry_check_version, pFp)
//GO(gcry_cipher_algo_info, 
//GO(gcry_cipher_algo_name, 
//GO(gcry_cipher_authenticate, 
//GO(gcry_cipher_checktag, 
//GO(gcry_cipher_close, 
//GO(gcry_cipher_ctl, 
//GO(gcry_cipher_decrypt, 
//GO(gcry_cipher_encrypt, 
//GO(gcry_cipher_get_algo_blklen, 
//GO(gcry_cipher_get_algo_keylen, 
//GO(gcry_cipher_gettag, 
//GO(gcry_cipher_info, 
//GO(gcry_cipher_map_name, 
//GO(gcry_cipher_mode_from_oid, 
//GO(gcry_cipher_open, 
//GO(gcry_cipher_setctr, 
//GO(gcry_cipher_setiv, 
//GO(gcry_cipher_setkey, 
GO(gcry_control, uFuNN)
//GO(gcry_create_nonce, 
//GO(gcry_ctx_release, 
//GO(gcry_ecc_get_algo_keylen, 
//GO(gcry_ecc_mul_point, 
//GO(gcry_err_code_from_errno, 
//GO(gcry_err_code_to_errno, 
//GO(gcry_err_make_from_errno, 
//GO(gcry_error_from_errno, 
//GO(gcry_free, 
//GO(gcry_get_config, 
//GO(gcry_is_secure, 
//GO(gcry_kdf_close, 
//GO(gcry_kdf_compute, 
//GO(gcry_kdf_derive, 
//GO(gcry_kdf_final, 
//GO(gcry_kdf_open, 
//GO(gcry_log_debug, 
//GO(gcry_log_debughex, 
//GO(gcry_log_debugmpi, 
//GO(gcry_log_debugpnt, 
//GO(gcry_log_debugsxp, 
//GO(gcry_mac_algo_info, 
//GO(gcry_mac_algo_name, 
//GO(gcry_mac_close, 
//GO(gcry_mac_ctl, 
//GO(gcry_mac_get_algo, 
//GO(gcry_mac_get_algo_keylen, 
//GO(gcry_mac_get_algo_maclen, 
//GO(gcry_mac_map_name, 
//GO(gcry_mac_open, 
//GO(gcry_mac_read, 
//GO(gcry_mac_setiv, 
//GO(gcry_mac_setkey, 
//GO(gcry_mac_verify, 
//GO(gcry_mac_write, 
//GO(gcry_malloc, 
//GO(gcry_malloc_secure, 
//GO(gcry_md_algo_info, 
GO(gcry_md_algo_name, pFi)
GO(gcry_md_close, vFp)
GO(gcry_md_copy, uFpp)
GO(gcry_md_ctl, pFpipL)
GO(gcry_md_debug, vFpp)
GO(gcry_md_enable, uFpi)
GO(gcry_md_extract, uFpipL)
GO(gcry_md_get_algo, iFp)
GO(gcry_md_get_algo_dlen, uFi)
GO(gcry_md_hash_buffer, vFippL)
GO(gcry_md_hash_buffers, uFiuppi)
//GO(gcry_md_info, 
GO(gcry_md_is_enabled, iFp)
GO(gcry_md_putc, vFpi)
GO(gcry_md_final, vFp)
GO(gcry_md_get_asnoid, uFipL)
GO(gcry_md_test_algo, uFi)
GO(gcry_md_is_secure, iFp)
GO(gcry_md_map_name, iFp)
GO(gcry_md_open, pFpiu)
GO(gcry_md_read, CFpi)
GO(gcry_md_reset, vFp)
GO(gcry_md_setkey, uFppL)
GO(gcry_md_write, vFppL)
//GO(gcry_mpi_abs, 
//GO(gcry_mpi_add, 
//GO(gcry_mpi_addm, 
//GO(gcry_mpi_add_ui, 
//GO(gcry_mpi_aprint, 
//GO(gcry_mpi_clear_bit, 
//GO(gcry_mpi_clear_flag, 
//GO(gcry_mpi_clear_highbit, 
//GO(gcry_mpi_cmp, 
//GO(gcry_mpi_cmp_ui, 
//GO(gcry_mpi_copy, 
//GO(gcry_mpi_div, 
//GO(gcry_mpi_dump, 
//GO(gcry_mpi_ec_add, 
//GO(gcry_mpi_ec_curve_point, 
//GO(gcry_mpi_ec_decode_point, 
//GO(gcry_mpi_ec_dup, 
//GO(gcry_mpi_ec_get_affine, 
//GO(gcry_mpi_ec_get_mpi, 
//GO(gcry_mpi_ec_get_point, 
//GO(gcry_mpi_ec_mul, 
//GO(gcry_mpi_ec_new, 
//GO(gcry_mpi_ec_set_mpi, 
//GO(gcry_mpi_ec_set_point, 
//GO(gcry_mpi_ec_sub, 
//GO(gcry_mpi_gcd, 
//GO(_gcry_mpi_get_const, 
//GO(gcry_mpi_get_flag, 
//GO(gcry_mpi_get_nbits, 
//GO(gcry_mpi_get_opaque, 
//GO(gcry_mpi_get_ui, 
//GO(gcry_mpi_invm, 
//GO(gcry_mpi_is_neg, 
//GO(gcry_mpi_lshift, 
//GO(gcry_mpi_mod, 
//GO(gcry_mpi_mul, 
//GO(gcry_mpi_mul_2exp, 
//GO(gcry_mpi_mulm, 
//GO(gcry_mpi_mul_ui, 
//GO(gcry_mpi_neg, 
GO(gcry_mpi_new, pFu)
//GO(gcry_mpi_point_copy, 
//GO(gcry_mpi_point_get, 
//GO(gcry_mpi_point_new, 
//GO(gcry_mpi_point_release, 
//GO(gcry_mpi_point_set, 
//GO(gcry_mpi_point_snatch_get, 
//GO(gcry_mpi_point_snatch_set, 
//GO(gcry_mpi_powm, 
GO(gcry_mpi_print, uFipLpp)
//GO(gcry_mpi_randomize, 
GO(gcry_mpi_release, vFp)
//GO(gcry_mpi_rshift, 
//GO(gcry_mpi_scan, 
//GO(gcry_mpi_set, 
//GO(gcry_mpi_set_bit, 
//GO(gcry_mpi_set_flag, 
//GO(gcry_mpi_set_highbit, 
//GO(gcry_mpi_set_opaque, 
//GO(gcry_mpi_set_opaque_copy, 
//GO(gcry_mpi_set_ui, 
//GO(gcry_mpi_snatch, 
GO(gcry_mpi_snew, pFu)
//GO(gcry_mpi_sub, 
//GO(gcry_mpi_subm, 
//GO(gcry_mpi_sub_ui, 
//GO(gcry_mpi_swap, 
//GO(gcry_mpi_test_bit, 
//GO(gcry_pk_algo_info, 
//GO(gcry_pk_algo_name, 
//GO(gcry_pk_ctl, 
GO(gcry_pk_decrypt, uFppp)
GO(gcry_pk_encrypt, uFppp)
//GO(gcry_pk_genkey, 
//GO(gcry_pk_get_curve, 
//GO(gcry_pk_get_keygrip, 
//GO(gcry_pk_get_nbits, 
//GO(gcry_pk_get_param, 
//GO(gcry_pk_hash_sign, 
//GO(gcry_pk_hash_verify, 
//GO(gcry_pk_map_name, 
//GO(gcry_pk_random_override_new, 
//GO(gcry_pk_sign, 
//GO(gcry_pk_testkey, 
//GO(gcry_pk_verify, 
//GO(gcry_prime_check, 
//GO(gcry_prime_generate, 
//GO(gcry_prime_group_generator, 
//GO(gcry_prime_release_factors, 
//GO(gcry_pubkey_get_sexp, 
//GO(gcry_random_add_bytes, 
//GO(gcry_random_bytes, 
//GO(gcry_random_bytes_secure, 
//GO(gcry_randomize, 
//GO(gcry_realloc, 
//GO(gcry_set_allocation_handler, 
//GO(gcry_set_fatalerror_handler, 
//GO(gcry_set_gettext_handler, 
//GO(gcry_set_log_handler, 
//GO(gcry_set_outofcore_handler, 
//GO(gcry_set_progress_handler, 
//GO(gcry_sexp_alist, 
//GO(gcry_sexp_append, 
GOM(gcry_sexp_build, uFEpppV)
GO(gcry_sexp_build_array, uFpppp)
//GO(gcry_sexp_cadr, 
//GO(gcry_sexp_canon_len, 
//GO(gcry_sexp_car, 
//GO(gcry_sexp_cdr, 
//GO(gcry_sexp_cons, 
//GO(gcry_sexp_create, 
//GO(gcry_sexp_dump, 
//GO(gcry_sexp_extract_param, 
GO(gcry_sexp_find_token, pFppL)
//GO(gcry_sexp_length, 
//GO(gcry_sexp_new, 
//GO(gcry_sexp_nth, 
//GO(gcry_sexp_nth_buffer, 
GO(gcry_sexp_nth_data, pFpip)
GO(gcry_sexp_nth_mpi, pFpii)
//GO(gcry_sexp_nth_string, 
//GO(gcry_sexp_prepend, 
GO(gcry_sexp_release, vFp)
//GO(gcry_sexp_sprint, 
//GO(gcry_sexp_sscan, 
//GO(gcry_sexp_vlist, 
//GO(gcry_strdup, 
GO(gcry_strerror, pFu)
GO(gcry_strsource, pFu)
//GO(gcry_xcalloc, 
//GO(gcry_xcalloc_secure, 
//GO(gcry_xmalloc, 
//GO(gcry_xmalloc_secure, 
//GO(gcry_xrealloc, 
//GO(gcry_xstrdup, 
