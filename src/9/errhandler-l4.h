/////////////// LP49 Err Handling ////////////////////////////////////////

#ifndef  __ERRHANDLER_L4_H__ 
#define  __ERRHANDLER_L4_H__ 

// On error, functions return either ONERR or ONERRNIL (=nil).
#define  ONERR     0xf001f001
#define  ONERRNIL  nil



#undef   waserror
#define  WASERROR()   (0)

#undef   poperror
#define  POPERROR()   

#define  ERROR_GOTO(err, label)		            \
   do { char ebuf[ERRMAX]; \
        snprint(ebuf, ERRMAX, "%s :%s()", err, __FUNCTION__); \
        kstrcpy(up->errstr, ebuf, ERRMAX);  \
        /* l4printf_c(7, "!\"%s\":%s().  ", err, __FUNCTION__);*/  \
        goto  label; } while(0) 

#define  ERROR_RETURN(err, val)		            \
   do { char ebuf[ERRMAX]; \
        snprint(ebuf, ERRMAX, "%s :%s()", err, __FUNCTION__); \
        kstrcpy(up->errstr, ebuf, ERRMAX);  \
        /* l4printf_c(7, "!\"%s\":%s(). ", err, __FUNCTION__);*/  \
        return val; } while(0) 


#define  NEXTERROR_GOTO(label)                                \
    do{ /* l4printf_c(7, "!:%s. ", __FUNCTION__); */ \
       goto label; } while(0)

#define  NEXTERROR_RETURN(val)                                \
    do{ /* l4printf_c(7, "!:%s. ", __FUNCTION__); */ \
       return  val; } while(0)


#define EXHAUSTED_GOTO(resource, label)				\
 do { l4printf_c(7, "no free %s:%s()\n", resource, __FUNCTION__); \
      goto label; } while(0)

#define EXHAUSTED_RETURN(resource, val)				\
 do { l4printf_c(7, "no free %s:%s()\n", resource, __FUNCTION__); \
      return val; } while(0)


#define IF_ERR_GOTO(x, y, label) \
    do {if(x == y)  goto label; } while(0) 

#define IF_GOTO(x, y, label) \
    do {if(x == y)  goto label; } while(0) 

#define IF_ERR_RETURN(x, y, val) \
    do {if(x == y)  return val;} while(0)  
 
#define IF_RETURN(x, y, val) \
    do {if(x == y)  return val;} while(0)  
 

/*======= Pattern 1 =====================
 *
 *  On error, a function returns ONERR (TYP==int) or nil (TYP==TT*)
 *
 *  TYP foo( ....)   
 *  {
 *    ....... 
 *   
 *    if (waserror()){   --> WASERROR()   i.e.  (0)
 *    _ERR_1:            --> Label is added. 
 *      ErrorHandling;
 *     
 *      nexterror( );   -->  NEXTERROR_RETURN(ONERR or nil);
 *                           i.e.  return -1; OR  return nil;
 *    }
 *
 *     ....
 *     error(emsg);    --> ERROR_GOTO(emsg, _ERR_1);  i.e. goto _ERR_1;
 *     ....
 *
 *     rc = may_error_function(...);
 *                      -->  IF_ERR_GOTO(rc, ONERR or nil, _ERR_1);
 *                           i.e. if (rc == errorvalue) goto _ERR_1;
 *     ......
 *
 *    poperror();      -->  POPERROR(); i.e. <empty>;
 *
 *    ........
 *
 *  }
 *    
 *****************************************/

/*======= Pattern 2: Nesting  =====================
 *
 *  // Error returns -1 (TYP==int) or nil (TYP==TT*)
 *  TYP foo( ....)   
 *  {
 *      ....... 
 *    
 *    if (waserror()) { --> WASERROR()  i.e. (0)  Start-of-outer-scope
 *    _ERR_1:
 *      ErrorHandling;
 *     
 *      nexterror( );    --> NEXTERROR_RETURN(ONERR-or-nil);
 *                           i.e. return <ONERR or nil>;
 *    }
 *
 *     .....
 *    error(ems);     -->  ERROR_GOTO(emsg, _ERR_1);  i.e. goto _ERR_1; 
 *    ....
 *
 *    rc = may_error_function(...);
 *                     --> IF_ERR_GOTO(rc, ONERR-or-nil, _ERR_1); 
 *                         i.e.  if (rc == ERROR) goto _ERR_1;
 *    ......
 *
 *
 *      if (waserror()){ --> WASERROR()   i.e. (0)  Start-of-inner-scope
 *      _ERR_2:
 *        ErrHandling
 *
 *	 nexterror();   --> NEXTERROR_GOTO(_ERR_1); i.e. goto _ERR_1;
 *      }
 *      ......
 *
 *      error(emsg);   --> ERROR_GOTO(emsg, _ERR_2);  i.e.  goto _ERR_2;   
 *      .....
 *
 *      rc = may_error_function(...);
 *                    -->  IF_ERR_GOTO(rc, ONERR-or-nil, _ERR_2);
 *                         i.e.  if (rc_is_ERROR) goto _ERR_2;
 *      ......
 *
 *     
 *      poperror();    -->  POPERROR(); i.e. <empty>;  End-of-inner-scope
 *
 *    .......	
 *    .....
 *    error(emsg);   -->  ERROR_GOTO(emsg, _ERR_1);  i.e. goto _ERR_1;
 *    ....
 *
 *    poperror();      --> POPERROR();  i.e. <empty>; End-of-outer-scope
 *
 *    ........
 *
 *  }
 *    
 */


/*======= Pattern 3: Function withoout err-handler ============
 *
 *  // Error returns -1 (TYPE==int) or nil (TYP==TT*)
 *  TYPE foo( ....)   
 *  {
 *     ....... 
 *
 *     ....
 *     error(emsg);   -->  ERROR_RETURN(emsg, ONERR-or-nil);
 *                           i.e.  return -1; OR  return nil;
 *     ....
 *
 *     rc = may_error_function(...);
 *     --> insert 
 *         IF_ERR_RETURN(rc, ONERR-or-nil, ONERR-or-nil); 
 *          i.e.  if (rc_is_ERROR)  return -1;  OR  return nil;
 *     ......
 *
 *    ........
 *
 *  }
 *    
 */


#endif  /* __ERRHANDLER_L4_H__  */
