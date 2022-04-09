# Server Code Instructions

- 1. First follow the instructions at https://github.com/eclipse-openj9/openj9/blob/e4cea4ada4bc94b6ed66f931b852aa00673095ea/doc/build-instructions/Build_Instructions_V11.md to build the openj9 repository.
- 2. Then import the files in this directory under openj9/runtime/compiler/runtime
- 3. Edit the openj9/runtime/compiler/runtime/CMakeLists.txt and openj9/runtime/compiler/build/files/common.mk files to make sure that the Server Code is included as part of the openj9 build
- 4. Now, you will need to use the functions provided in MetricServer.hpp to create/start/stop/destroy the MetricServer thread
- 5. Edit the following files: runtime/compiler/control/HookedByTheJit.cpp, runtime/compiler/control/rossa.cpp, runtime/compiler/control/DLLMain.cpp, and runtime/compiler/env/VMJ9.h.
- 6. For VMJ9.h, you need to declare the MetricServer class and you need to create a pointer to the MetricServer in JITPrivateConfig. We need this pointer to kill the thread later on.
- 7. For rossa.cpp, DLLMain.cpp, and HookedByTheJit.cpp, you will need to use the MetricServer functions defined in MetricServer.hpp to create/start/stop/destroy the MetricServer thread appropiately. Note: that it is very very similar to the listener thread for compilation requests.
- 8. Once finished, go to the home directory and run `make all`.
- 9. Now the build should pass and JITServer can now be run with the MetricServer thread being used to export metrics.
- 10. The following options can be set to configure the MetricServer:
  - `-XX:MetricServerPort=<port>` is the port that the MetricServer listens to for HTTP requests (default is 7390, must be between 1024 and 65535).
  - `-XX:MetricServerTimeoutMs=<timeout>` is the timeout (in milliseconds) that the MetricServer uses for polling (Default is 100).
  - `-XX:MetricServerMaxConnections=<maxconnections>` is the max number of connections that the MetricServer can handle at once (Default is 10).
  - `-XX:MetricServerHttpsPort=<httpsport>` is the port that the MetricServer listens to for HTTPS requests (default is 7391, must be between 1024 and 65535).
  
  
