//This constant is used to define the default size of the receive and send buffers for the server. The receive and send buffer will increase in size by this amount if there is unsufficient space.
static unsigned int const default_buffer_size = 4096;

//This constant is used to define the maximum size of the receive and send buffer. If the maximum is reached, then the request/response is aborted.
static unsigned int const max_buffer_size = 4096;

//These constants represent the various return codes that our main function can return.
enum Codes {Success = 0, TooFewArgs = -1, TooManyArgs = -2, DatabaseFailed = -3, ServerFailed = -4};
