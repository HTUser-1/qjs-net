macro(build_libwebsockets)
  message("-- Building LIBWEBSOCKETS from source")

  if(NOT DEFINED LIBWEBSOCKETS_C_FLAGS)
    message(
      FATAL_ERROR "Please set LIBWEBSOCKETS_C_FLAGS before including this file."
    )
  endif()

  set(LWS_WITHOUT_TESTAPPS TRUE)
  set(LWS_WITHOUT_TEST_SERVER TRUE)
  set(LWS_WITHOUT_TEST_PING TRUE)
  set(LWS_WITHOUT_TEST_CLIENT TRUE)
  set(LWS_LINK_TESTAPPS_DYNAMIC OFF CACHE BOOL "link test apps dynamic")
  set(LWS_WITH_STATIC ON CACHE BOOL "build libwebsockets static library")
  set(LWS_HAVE_LIBCAP FALSE CACHE BOOL "have libcap")

  # include: libwebsockets find_package(libwebsockets)
  unset(LIBWEBSOCKETS_INCLUDE_DIR)
  unset(LIBWEBSOCKETS_INCLUDE_DIR CACHE)
  set(LIBWEBSOCKETS_INCLUDE_DIR
      ${CMAKE_CURRENT_SOURCE_DIR}/libwebsockets/include
      ${CMAKE_CURRENT_BINARY_DIR}/libwebsockets
      ${CMAKE_CURRENT_BINARY_DIR}/libwebsockets/include)
  set(LIBWEBSOCKETS_FOUND ON CACHE BOOL "found libwebsockets")
  set(LIBWEBSOCKETS_LIBRARIES "brotlienc;brotlidec;cap")
  if(OPENSSL_LIBRARIES)
    set(LIBWEBSOCKETS_LIBRARIES
        "${OPENSSL_LIBRARIES};${LIBWEBSOCKETS_LIBRARIES}")
  else(OPENSSL_LIBRARIES)
    if(MBEDTLS_LIBRARIES)
      set(LIBWEBSOCKETS_LIBRARIES
          "${MBEDTLS_LIBRARIES};${LIBWEBSOCKETS_LIBRARIES}")
    endif(MBEDTLS_LIBRARIES)
  endif(OPENSSL_LIBRARIES)

  set(LIBWEBSOCKETS_ARGS -DLWS_WITH_SSL:BOOL=ON -DLWS_WITH_WOLFSSL:BOOL=OFF
                         -DLWS_WITH_MBEDTLS:BOOL=OFF)
  if(CMAKE_TOOLCHAIN_FILE)
    set(LIBWEBSOCKETS_ARGS
        ${LIBWEBSOCKETS_ARGS}
        -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_TOOLCHAIN_FILE})
  endif(CMAKE_TOOLCHAIN_FILE)

  if(OPENSSL_LIBRARIES)
    set(LIBWEBSOCKETS_ARGS
        "${LIBWEBSOCKETS_ARGS} -DLWS_OPENSSL_LIBRARIES:STRING=${OPENSSL_LIBRARIES} -DOPENSSL_LIBRARIES:STRING=${OPENSSL_LIBRARIES}"
    )
  endif(OPENSSL_LIBRARIES)
  if(OPENSSL_LIBRARY)
    set(LIBWEBSOCKETS_ARGS
        "${LIBWEBSOCKETS_ARGS} -DLWS_OPENSSL_LIBRARIES:STRING=${OPENSSL_LIBRARY} -DOPENSSL_LIBRARIES:STRING=${OPENSSL_LIBRARY}"
    )
  endif(OPENSSL_LIBRARY)
  if(OPENSSL_LIBRARY_DIR)
    set(LIBWEBSOCKETS_ARGS
        "${LIBWEBSOCKETS_ARGS} -DCMAKE_LIBRARY_PATH:STRING=${OPENSSL_LIBRARY_DIR} -DOPENSSL_LIBRARY_DIR:PATH=${OPENSSL_LIBRARY_DIR} -DLWS_OPENSSL_LIBRARY_DIR:PATH=${OPENSSL_LIBRARY_DIR}"
    )
  endif(OPENSSL_LIBRARY_DIR)
  if(OPENSSL_INCLUDE_DIR)
    set(LIBWEBSOCKETS_ARGS
        "${LIBWEBSOCKETS_ARGS} -DCMAKE_INCLUDE_PATH:STRING=${OPENSSL_INCLUDE_DIR} -DLWS_OPENSSL_INCLUDE_DIRS:STRING=${OPENSSL_INCLUDE_DIR} -DLWS_OPENSSL_INCLUDE_DIRS:STRING=${OPENSSL_INCLUDE_DIR} -DOPENSSL_INCLUDE_DIRS:STRING=${OPENSSL_INCLUDE_DIR} -DOPENSSL_INCLUDE_DIRS:STRING=${OPENSSL_INCLUDE_DIR}"
    )
  endif(OPENSSL_INCLUDE_DIR)
  if(OPENSSL_INCLUDE_DIRS)
    set(LIBWEBSOCKETS_ARGS
        "${LIBWEBSOCKETS_ARGS} -DCMAKE_INCLUDE_PATH:STRING=${OPENSSL_INCLUDE_DIRS} -DLWS_OPENSSL_INCLUDE_DIRS:STRING=${OPENSSL_INCLUDE_DIRS} -DLWS_OPENSSL_INCLUDE_DIRS:STRING=${OPENSSL_INCLUDE_DIR} -DOPENSSL_INCLUDE_DIRS:STRING=${OPENSSL_INCLUDE_DIRS} -DOPENSSL_INCLUDE_DIRS:STRING=${OPENSSL_INCLUDE_DIR}"
    )
  endif(OPENSSL_INCLUDE_DIRS)
  if(OPENSSL_EXECUTABLE)
    set(LIBWEBSOCKETS_ARGS
        "${LIBWEBSOCKETS_ARGS} -DOPENSSL_EXECUTABLE:FILEPATH=${OPENSSL_EXECUTABLE}"
    )
  endif(OPENSSL_EXECUTABLE)

  set(LIBWEBSOCKETS_LIBRARIES
      "${CMAKE_CURRENT_BINARY_DIR}/libwebsockets/lib/libwebsockets_static.a;${LIBWEBSOCKETS_LIBRARIES}"
  )

  if(WIN32)
    set(LIBWEBSOCKETS_LIBRARIES "${LIBWEBSOCKETS_LIBRARIES};crypt32")
  endif(WIN32)
  #    "${CMAKE_CURRENT_BINARY_DIR}/libwebsockets/lib/libwebsockets.a;${LIBWEBSOCKETS_LIBRARIES}"
  #else(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/libwebsockets/lib/libwebsockets.a")
  #  set(LIBWEBSOCKETS_LIBRARIES "websockets;${LIBWEBSOCKETS_LIBRARIES}")
  #endif(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/libwebsockets/lib/libwebsockets.a")
  set(LIBWEBSOCKETS_LIBRARIES "${LIBWEBSOCKETS_LIBRARIES}"
      CACHE STRING "libwebsockets libraries")

  set(LIBWEBSOCKETS_INCLUDE_DIR "${LIBWEBSOCKETS_INCLUDE_DIR}"
      CACHE PATH "libwebsockets include directory")
  set(LIBWEBSOCKETS_LIBRARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/libwebsockets/lib
      CACHE PATH "libwebsockets library directory")
  # add_subdirectory(libwebsockets ${CMAKE_CURRENT_BINARY_DIR}/libwebsockets)
  include(ExternalProject)

  if(WITH_WOLFSSL)
    set(LIBWEBSOCKETS_ARGS
        -DLWS_WITH_WOLFSSL:BOOL=ON
        #-DLWS_HAVE_EVP_PKEY_new_raw_private_key:BOOL=ON
        -DLWS_WITH_NETWORK:BOOL=OFF
        -DLWS_WITH_DIR:BOOL=OFF
        # -DLWS_WITH_RANGES:BOOL=ON
        -DLWS_WITH_JOSE:BOOL=OFF
        -DLWS_WITH_ACCESS_LOG:BOOL=OFF
        -DLWS_WITH_SSL:BOOL=OFF
        -DLWS_WITH_MBEDTLS:BOOL=OFF)
    find_package(WOLFSSL NAMES wolfssl PATHS "${WOLFSSL_DIR}" REQUIRED)

    if(WOLFSSL_FOUND)

      message("WOLFSSL_CONFIG: ${WOLFSSL_CONFIG}")
      include(${WOLFSSL_CONFIG})

      dirname(WOLFSSL_DIR "${WOLFSSL_CONFIG}")
      message("WOLFSSL_DIR: ${WOLFSSL_DIR}")

      include(${WOLFSSL_DIR}/wolfssl-config.cmake)

      get_target_property(pkgcfg_lib_WOLFSSL_wolfssl wolfssl
                          IMPORTED_LOCATION_RELWITHDEBINFO)

      dirname(WOLFSSL_LIBRARIES_DIR ${pkgcfg_lib_WOLFSSL_wolfssl})
      string(REGEX REPLACE "/lib.*" "/include" WOLFSSL_INCLUDE_DIR
                           "${WOLFSSL_LIBRARIES_DIR}")

      set(WOLFSSL_LIBRARIES ${WOLFSSL_LIBRARIES} ${pkgcfg_lib_WOLFSSL_wolfssl})
      #list(FILTER WOLFSSL_LIBRARIES EXCLUDE REGEX wolfssl_shared)
      message("WOLFSSL_INCLUDE_DIR: ${WOLFSSL_INCLUDE_DIR}")
      message("WOLFSSL_LIBRARIES_DIR: ${WOLFSSL_LIBRARIES_DIR}")

      dump(WOLFSSL_FOUND WOLFSSL_LIBRARIES WOLFSSL_LIBRARIES_DIR
           WOLFSSL_INCLUDE_DIR)
      set(LIBWEBSOCKETS_ARGS
          ${LIBWEBSOCKETS_ARGS}
          -DLWS_WOLFSSL_LIBRARIES:STRING=${WOLFSSL_LIBRARIES}
          -DLWS_WOLFSSL_INCLUDE_DIRS:STRING=${WOLFSSL_INCLUDE_DIR})

    endif(WOLFSSL_FOUND)

  elseif(WITH_WOLFSSL)
    set(LIBWEBSOCKETS_ARGS -DLWS_ROLE_H2:BOOL=ON -DLWS_WITH_RANGES:BOOL=ON)
    if(1)

    elseif(WITH_SSL)
      if(WITH_MBEDTLS)
        set(LIBWEBSOCKETS_ARGS
            -DLWS_WITH_MBEDTLS:BOOL=ON -DLWS_WITH_WOLFSSL:BOOL=OFF
            -DLWS_WITH_SSL:BOOL=OFF)
        set(LIBWEBSOCKETS_ARGS
            ${LIBWEBSOCKETS_ARGS}
            -DLWS_MBEDTLS_LIBRARIES:STRING=${MBEDTLS_LIBRARIES}
            -DLWS_MBEDTLS_INCLUDE_DIRS:STRING=${MBEDTLS_INCLUDE_DIR})
      endif()
    endif()
  endif()

  if(ZLIB_LIBRARY_RELEASE)
    set(LIBWEBSOCKETS_ARGS
        "${LIBWEBSOCKETS_ARGS} -DLWS_ZLIB_LIBRARIES:PATH=${ZLIB_LIBRARY_RELEASE}"
    )
  endif(ZLIB_LIBRARY_RELEASE)

  if(ZLIB_INCLUDE_DIR)
    set(LIBWEBSOCKETS_ARGS
        "${LIBWEBSOCKETS_ARGS} -DLWS_ZLIB_INCLUDE_DIRS:PATH=${ZLIB_INCLUDE_DIR}"
    )
  endif(ZLIB_INCLUDE_DIR)

  #if("${LWS_HAVE_HMAC_CTX_new}" STREQUAL "")
  #set(LWS_HAVE_HMAC_CTX_new 1 CACHE STRING "Have HMAC_CTX_new")
  #endif("${LWS_HAVE_HMAC_CTX_new}" STREQUAL "")
  #
  #if("${LWS_HAVE_EVP_MD_CTX_free}" STREQUAL "")
  #set(LWS_HAVE_EVP_MD_CTX_free 1 CACHE STRING "Have EVP_MD_CTX_free")
  #endif("${LWS_HAVE_EVP_MD_CTX_free}" STREQUAL "")
  #
  if("${LWS_HAVE_X509_VERIFY_PARAM_set1_host}" STREQUAL "")
    set(LWS_HAVE_X509_VERIFY_PARAM_set1_host 1
        CACHE STRING "Have X509_VERIFY_PARAM_set1_host")
  endif("${LWS_HAVE_X509_VERIFY_PARAM_set1_host}" STREQUAL "")
  #[[if(CMAKE_PREFIX_PATH OR OPENSSL_ROOT_DIR)
    set(LIBWEBSOCKETS_ARGS "${LIBWEBSOCKETS_ARGS} -DCMAKE_PREFIX_PATH:STRING=${CMAKE_PREFIX_PATH};${OPENSSL_ROOT_DIR}")
  endif()
  if(CMAKE_LIBRARY_PATH OR OPENSSL_LIBRARY_DIR)
    set(LIBWEBSOCKETS_ARGS "${LIBWEBSOCKETS_ARGS} -DCMAKE_LIBRARY_PATH:STRING=${CMAKE_LIBRARY_PATH};${OPENSSL_LIBRARY_DIR}")
  endif()
  if(CMAKE_INCLUDE_PATH OR OPENSSL_INCLUDE_DIR)
    set(LIBWEBSOCKETS_ARGS "${LIBWEBSOCKETS_ARGS} -DCMAKE_INCLUDE_PATH:STRING=${CMAKE_INCLUDE_PATH};${OPENSSL_INCLUDE_DIR}")
  endif()]]

  string(REGEX REPLACE "[ \t\n]" "\n\t" ARGS "${LIBWEBSOCKETS_ARGS}")
  message("libwebsockets configuration arguments:\n\t${ARGS}")
  string(REGEX REPLACE "[ ]" ";" LIBWEBSOCKETS_ARGS "${LIBWEBSOCKETS_ARGS}")

  ExternalProject_Add(
    libwebsockets
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libwebsockets
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/libwebsockets
    PREFIX libwebsockets
    CMAKE_ARGS
      "-DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}"
      "-DCMAKE_C_FLAGS:STRING=${LIBWEBSOCKETS_C_FLAGS}"
      #"-DCMAKE_C_FLAGS:STRING=${LIBWEBSOCKETS_C_FLAGS} -DSSL_CTRL_SET_TLSEXT_HOSTNAME"
      "-DCMAKE_VERBOSE_MAKEFILE:BOOL=${CMAKE_VERBOSE_MAKEFILE}"
      "-DCMAKE_INSTALL_RPATH:STRING=${MBEDTLS_LIBRARY_DIR}"
      "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
      "-DCMAKE_LIBRARY_PATH:PATH=${CMAKE_LIBRARY_PATH}"
      #"-DCMAKE_REQUIRED_LIBRARIES:STRING=tls;ssl;crypto;pthread;dl"
      #"-DCMAKE_REQUIRED_LINK_OPTIONS:STRING=-L${OPENSSL_LIBRARY_DIR}"
      -DBUILD_TESTING:BOOL=OFF
      -DCMAKE_COLOR_MAKEFILE:BOOL=ON
      -DCOMPILER_IS_CLANG:BOOL=OFF
      -DDISABLE_WERROR:BOOL=ON
      -DLWS_AVOID_SIGPIPE_IGN:BOOL=OFF
      -DLWS_CLIENT_HTTP_PROXYING:BOOL=ON
      -DLWS_FALLBACK_GETHOSTBYNAME:BOOL=ON
      -DLWS_FOR_GITOHASHI:BOOL=OFF
      -DLWS_HTTP_HEADERS_ALL:BOOL=OFF
      -DLWS_IPV6:BOOL=OFF
      -DLWS_LOGS_TIMESTAMP:BOOL=ON
      -DLWS_LOG_TAG_LIFECYCLE:BOOL=ON
      -DLWS_REPRODUCIBLE:BOOL=ON
      -DLWS_ROLE_DBUS:BOOL=OFF
      -DLWS_ROLE_MQTT:BOOL=OFF
      -DLWS_ROLE_RAW_FILE:BOOL=ON
      -DLWS_ROLE_RAW_PROXY:BOOL=ON
      -DLWS_ROLE_WS:BOOL=ON
      -DLWS_SSL_CLIENT_USE_OS_CA_CERTS:BOOL=ON
      -DLWS_SSL_SERVER_WITH_ECDH_CERT:BOOL=OFF
      -DLWS_STATIC_PIC:BOOL=ON
      -DLWS_SUPPRESS_DEPRECATED_API_WARNINGS:BOOL=ON
      -DLWS_TLS_LOG_PLAINTEXT_RX:BOOL=OFF
      -DLWS_TLS_LOG_PLAINTEXT_TX:BOOL=OFF
      -DLWS_UNIX_SOCK:BOOL=ON
      -DLWS_WITHOUT_BUILTIN_SHA1:BOOL=OFF
      -DLWS_WITHOUT_CLIENT:BOOL=OFF
      -DLWS_WITHOUT_DAEMONIZE:BOOL=ON
      -DLWS_WITHOUT_EVENTFD:BOOL=OFF
      -DLWS_WITHOUT_EXTENSIONS:BOOL=OFF
      -DLWS_WITHOUT_SERVER:BOOL=OFF
      -DLWS_WITHOUT_TESTAPPS:BOOL=ON
      -DLWS_WITHOUT_TEST_SERVER:BOOL=OFF
      -DLWS_WITH_ACCESS_LOG:BOOL=OFF
      -DLWS_WITH_ACME:BOOL=ON
      -DLWS_WITH_ALSA:BOOL=OFF
      -DLWS_WITH_ASAN:BOOL=OFF
      -DLWS_WITH_BORINGSSL:BOOL=OFF
      -DLWS_WITH_BUNDLED_ZLIB:BOOL=OFF
      -DLWS_WITH_CGI:BOOL=OFF
      -DLWS_WITH_CONMON:BOOL=ON
      -DLWS_WITH_CUSTOM_HEADERS:BOOL=ON
      -DLWS_WITH_DISKCACHE:BOOL=OFF
      -DLWS_WITH_DISTRO_RECOMMENDED:BOOL=OFF
      -DLWS_WITH_DRIVERS:BOOL=OFF
      -DLWS_WITH_ESP32:BOOL=OFF
      -DLWS_WITH_EVLIB_PLUGINS:BOOL=OFF
      -DLWS_WITH_EXPORT_LWSTARGETS:BOOL=ON
      -DLWS_WITH_EXTERNAL_POLL:BOOL=ON
      -DLWS_WITH_FANALYZER:BOOL=OFF
      -DLWS_WITH_FILE_OPS:BOOL=ON
      -DLWS_WITH_FSMOUNT:BOOL=ON
      -DLWS_WITH_FTS:BOOL=OFF
      -DLWS_WITH_GCOV:BOOL=OFF
      -DLWS_WITH_GENCRYPTO:BOOL=OFF
      -DLWS_WITH_GLIB:BOOL=OFF
      -DLWS_WITH_GTK:BOOL=OFF
      -DLWS_WITH_HTTP2:BOOL=${WITH_HTTP2}
      -DLWS_WITH_HTTP_BASIC_AUTH:BOOL=ON
      -DLWS_WITH_HTTP_BROTLI:BOOL=ON
      -DLWS_WITH_HTTP_PROXY:BOOL=ON
      -DLWS_WITH_HTTP_STREAM_COMPRESSION:BOOL=ON
      -DLWS_WITH_HTTP_UNCOMMON_HEADERS:BOOL=ON
      -DLWS_WITH_HUBBUB:BOOL=OFF
      #-DLWS_WITH_LEJP:BOOL=ON
      #-DLWS_WITH_LEJP_CONF:BOOL=OFF
      -DLWS_WITH_LIBEV:BOOL=OFF
      -DLWS_WITH_LIBEVENT:BOOL=OFF
      -DLWS_WITH_LIBUV:BOOL=OFF
      -DLWS_WITH_LWSAC:BOOL=ON
      -DLWS_WITH_LWSWS:BOOL=OFF
      -DLWS_WITH_LWS_DSH:BOOL=OFF
      -DLWS_WITH_MINIMAL_EXAMPLES:BOOL=OFF
      -DLWS_WITH_MINIZ:BOOL=OFF
      -DLWS_WITH_NETLINK:BOOL=ON
      -DLWS_WITH_NO_LOGS:BOOL=OFF
      -DLWS_WITH_PEER_LIMITS:BOOL=OFF
      -DLWS_WITH_PLUGINS:BOOL=ON
      -DLWS_WITH_PLUGINS_API:BOOL=OFF
      -DLWS_WITH_SDEVENT:BOOL=OFF
      -DLWS_WITH_SECURE_STREAMS:BOOL=OFF
      -DLWS_WITH_SECURE_STREAMS_AUTH_SIGV4:BOOL=OFF
      -DLWS_WITH_SECURE_STREAMS_CPP:BOOL=OFF
      -DLWS_WITH_SECURE_STREAMS_PROXY_API:BOOL=OFF
      -DLWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY:BOOL=OFF
      -DLWS_WITH_SECURE_STREAMS_SYS_AUTH_API_AMAZON_COM:BOOL=OFF
      -DLWS_WITH_SELFTESTS:BOOL=ON
      -DLWS_WITH_SEQUENCER:BOOL=OFF
      -DLWS_WITH_SHARED:BOOL=OFF
      -DLWS_WITH_SOCKS5:BOOL=ON
      -DLWS_WITH_SPAWN:BOOL=OFF
      -DLWS_WITH_SQLITE3:BOOL=OFF
      -DLWS_WITH_STATIC:BOOL=ON
      -DLWS_WITH_STRUCT_JSON:BOOL=OFF
      -DLWS_WITH_STRUCT_SQLITE3:BOOL=OFF
      -DLWS_WITH_SUL_DEBUGGING:BOOL=OFF
      -DLWS_WITH_SYS_ASYNC_DNS:BOOL=OFF
      -DLWS_WITH_SYS_DHCP_CLIENT:BOOL=OFF
      -DLWS_WITH_SYS_FAULT_INJECTION:BOOL=OFF
      -DLWS_WITH_SYS_METRICS:BOOL=OFF
      -DLWS_WITH_SYS_NTPCLIENT:BOOL=OFF
      -DLWS_WITH_SYS_SMD:BOOL=ON
      -DLWS_WITH_SYS_STATE:BOOL=ON
      -DLWS_WITH_THREADPOOL:BOOL=ON
      -DLWS_WITH_TLS_SESSIONS:BOOL=ON
      -DLWS_WITH_UDP:BOOL=ON
      -DLWS_WITH_ULOOP:BOOL=OFF
      -DLWS_WITH_UNIX_SOCK:BOOL=ON
      -DLWS_WITH_ZIP_FOPS:BOOL=ON
      -DLWS_WITH_ZLIB:BOOL=ON
    CMAKE_CACHE_ARGS
      ${LIBWEBSOCKETS_ARGS}
      -DLWS_HAVE_LIBCAP:BOOL=FALSE
      -DLWS_WITH_PLUGINS_API:BOOL=${LWS_WITH_PLUGINS_API}
      -DLWS_WITH_PLUGINS:BOOL=${LWS_WITH_PLUGINS}
      -DLWS_WITH_PLUGINS_BUILTIN:BOOL=${LWS_WITH_PLUGINS_BUILTIN}
      #"-DLWS_HAVE_X509_VERIFY_PARAM_set1_host:STRING=1"
      #"-DLWS_HAVE_X509_VERIFY_PARAM_set1_host_sym:STRING=1"
    INSTALL_COMMAND ""
    #LOG_DOWNLOAD ON
    USES_TERMINAL_DOWNLOAD ON
    #LOG_CONFIGURE ON
    USES_TERMINAL_CONFIGURE ON
    #LOG_BUILD ON
    USES_TERMINAL_BUILD ON
    #LOG_OUTPUT_ON_FAILURE ON
  )
  # ExternalProject_Get_Property(libwebsockets CMAKE_CACHE_DEFAULT_ARGS)
  #message("CMAKE_CACHE_DEFAULT_ARGS of libwebsockets = ${CMAKE_CACHE_DEFAULT_ARGS}")

  #link_directories("${CMAKE_CURRENT_BINARY_DIR}/libwebsockets/lib")

  if(ARGN)
    ExternalProject_Add_StepDependencies(libwebsockets build ${ARGN})
  endif(ARGN)

endmacro(build_libwebsockets)
