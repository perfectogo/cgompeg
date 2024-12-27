#include "./core/cgompeg.h"
#include <libavutil/log.h>
#include <stdio.h>

int main(int argc, char **argv) {

  if (argc < 3) {
    fprintf(stderr, "Usage: %s <input_file> <output_file>\n", argv[0]);
    return 1;
  }

  av_log_set_level(AV_LOG_QUIET);

  const char *input_file = argv[1];
  const char *output_file = argv[2];

  int result = cmd(input_file, output_file);
  {
    if (result != 0) {
      fprintf(stderr, "Error: HLS conversion failed.\n");
      return 1;
    }
  }

  return 0;
}
