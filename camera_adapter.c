#include <SDL/SDL.h>
#include <assert.h>

#include <stdio.h>
#include <assert.h>

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <asm/types.h>          /* for videodev2.h */

#include <linux/videodev2.h>

#include "shared_variables.h"
#include "filter_utils.h"

#define mask32(BYTE) (*(uint32_t *)(uint8_t [4]){ [BYTE] = 0xff })

#define CLEAR(x) memset (&(x), 0, sizeof (x))

#define COLOR_GET_RED(color)   ((color >> 16) & 0xFF)
#define COLOR_GET_GREEN(color) ((color >> 8) & 0xFF)
#define COLOR_GET_BLUE(color)  (color & 0xFF)

typedef enum
{
  IO_METHOD_READ,
  IO_METHOD_MMAP,
  IO_METHOD_USERPTR,
} io_method;

struct buffer
{
  void *start;
  size_t length;
};

static char *dev_name = NULL;
static io_method io = IO_METHOD_MMAP;
static int fd = -1;
struct buffer *buffers = NULL;
static unsigned int n_buffers = 0;

static int filter_no = 0;

static uint8_t *buffer_sdl;
SDL_Surface *data_sf;

/*-------------------------------- functions -------------------------------------*/

static void errno_exit(const char *s)
{
  fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));

  exit(EXIT_FAILURE);
}

static int xioctl(int fd, int request, void *arg)
{
  int r;

  do
  {
      r = ioctl(fd, request, arg);
  }
  while (-1 == r && EINTR == errno);

  return r;
}

static void render(SDL_Surface * sf)
{
  SDL_Surface *screen = SDL_GetVideoSurface();
  if (SDL_BlitSurface(sf, NULL, screen, NULL) == 0)
    SDL_UpdateRect(screen, 0, 0, 0, 0);
}

static uint32_t YCbCr_to_RGB[256][256][256];

static void generate_YCbCr_to_RGB_lookup()
{
  int y;
  int cb;
  int cr;

  for (y = 0; y < 256; y++)
  {
    for (cb = 0; cb < 256; cb++)
    {
      for (cr = 0; cr < 256; cr++)
      {
        double Y = (double)y;
        double Cb = (double)cb;
        double Cr = (double)cr;

        int R = (int)(Y+1.40200*(Cr - 0x80));
        int G = (int)(Y-0.34414*(Cb - 0x80)-0.71414*(Cr - 0x80));
        int B = (int)(Y+1.77200*(Cb - 0x80));

        R = max(0, min(255, R));
        G = max(0, min(255, G));
        B = max(0, min(255, B));

        YCbCr_to_RGB[y][cb][cr] = R << 16 | G << 8 | B;
      }
    }
  }
}

static void inline YUV422_to_RGB(uint8_t * output, const uint8_t * input)
{
  uint8_t y0 = input[0];
  uint8_t cb = input[1];
  uint8_t y1 = input[2];
  uint8_t cr = input[3];

  uint32_t rgb = YCbCr_to_RGB[y0][cb][cr];
  output[0] = COLOR_GET_RED(rgb);
  output[1] = COLOR_GET_GREEN(rgb);
  output[2] = COLOR_GET_BLUE(rgb);

  rgb = YCbCr_to_RGB[y1][cb][cr];
  output[3] = COLOR_GET_RED(rgb);
  output[4] = COLOR_GET_GREEN(rgb);
  output[5] = COLOR_GET_BLUE(rgb);
}


static void process_image(const void *p)
{

  const uint8_t *buffer_yuv = p;

  size_t x;
  size_t y;

  for (y = 0; y < HEIGHT; y++)
    for (x = 0; x < WIDTH; x+=2)
      YUV422_to_RGB(buffer_sdl + (y * WIDTH + x) * 3,
                    buffer_yuv + (y * WIDTH + x)*2);


  filters[filter_no]->func(&(filters[filter_no]->arg), buffer_sdl); //apply selected filter

  render(data_sf);
}

static int read_frame(void)
{
  struct v4l2_buffer buf;
  unsigned int i;

  CLEAR(buf);

  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;

  if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf))
  {
    switch (errno)
    {
    case EAGAIN:
        return 0;

    case EIO:
        /* Could ignore EIO, see spec. */

        /* fall through */

    default:
        errno_exit("VIDIOC_DQBUF");
    }
  }

  assert(buf.index < n_buffers);

  process_image(buffers[buf.index].start);
  if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
      errno_exit("VIDIOC_QBUF");
  return 0;
}

static void mainloop(void)
{
  SDL_Event event;
  fd_set fds;
  int running = 1;
  struct timeval tv;
  int r;
  for (;running==1;)
  {
    while (SDL_PollEvent(&event) != 0){

      if (event.type == SDL_QUIT)
        return;
      else {
        SDLKey keyPressed = event.key.keysym.sym;
        switch (keyPressed)
        {
          case SDLK_ESCAPE:
            running = 0;
            break;
          case SDLK_LEFT:
            filter_no =  (filter_no -1 < 0) ? FILTER_COUNT - 1 : (filter_no -1 );
            SDL_WM_SetCaption(filter_names[filter_no], NULL);
            break;
          case SDLK_RIGHT:
            filter_no = (filter_no + 1)%FILTER_COUNT;
            SDL_WM_SetCaption(filter_names[filter_no], NULL);
            break;
        }
      }
    }


    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    /* Timeout. */
    tv.tv_sec = 1;
    tv.tv_usec = 1000;

    r = select(fd + 1, &fds, NULL, NULL, &tv);

    if (-1 == r)
    {
        if (EINTR == errno)
            continue;

        errno_exit("select");
    }

    read_frame();
  }
}

static void stop_capturing(void)
{
  enum v4l2_buf_type type;

  switch (io)
  {
  case IO_METHOD_READ:
    /* Nothing to do. */
    break;

  case IO_METHOD_MMAP:
  case IO_METHOD_USERPTR:
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
        errno_exit("VIDIOC_STREAMOFF");

    break;
  }
}

static void start_capturing(void)
{
  unsigned int i;
  enum v4l2_buf_type type;
  struct v4l2_buffer buf;

  for (i = 0; i < n_buffers; ++i)
  {
    CLEAR(buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;

    if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
        errno_exit("VIDIOC_QBUF");
  }

  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
    errno_exit("VIDIOC_STREAMON");

}

static void uninit_device(void)
{
  unsigned int i;

  switch (io)
  {
  case IO_METHOD_READ:
      free(buffers[0].start);
      break;

  case IO_METHOD_MMAP:
      for (i = 0; i < n_buffers; ++i)
          if (-1 == munmap(buffers[i].start, buffers[i].length))
              errno_exit("munmap");
      break;

  case IO_METHOD_USERPTR:
      for (i = 0; i < n_buffers; ++i)
          free(buffers[i].start);
      break;
  }

  free(buffers);
}

static void init_read(unsigned int buffer_size)
{
  buffers = calloc(1, sizeof(*buffers));

  if (!buffers)
  {
      fprintf(stderr, "Out of memory\n");
      exit(EXIT_FAILURE);
  }

  buffers[0].length = buffer_size;
  buffers[0].start = malloc(buffer_size);

  if (!buffers[0].start)
  {
      fprintf(stderr, "Out of memory\n");
      exit(EXIT_FAILURE);
  }
}

static void init_mmap(void)
{
  struct v4l2_requestbuffers req;

  CLEAR(req);

  req.count = 4;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;

  if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req))
  {
    if (EINVAL == errno)
    {
      fprintf(stderr, "%s does not support "
              "memory mapping\n", dev_name);
      exit(EXIT_FAILURE);
    }
    else
    {
      errno_exit("VIDIOC_REQBUFS");
    }
  }

  if (req.count < 2)
  {
    fprintf(stderr, "Insufficient buffer memory on %s\n", dev_name);
    exit(EXIT_FAILURE);
  }

  buffers = calloc(req.count, sizeof(*buffers));

  if (!buffers)
  {
    fprintf(stderr, "Out of memory\n");
    exit(EXIT_FAILURE);
  }

  for (n_buffers = 0; n_buffers < req.count; ++n_buffers)
  {
    struct v4l2_buffer buf;

    CLEAR(buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = n_buffers;

    if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
      errno_exit("VIDIOC_QUERYBUF");

    buffers[n_buffers].length = buf.length;
    buffers[n_buffers].start = mmap(NULL /* start anywhere */ ,
                                    buf.length, PROT_READ | PROT_WRITE  /* required
                                                                         */ ,
                                    MAP_SHARED /* recommended */ ,
                                    fd, buf.m.offset);

    if (MAP_FAILED == buffers[n_buffers].start)
      errno_exit("mmap");
  }
}


static void init_device(void)
{
  struct v4l2_capability cap;
  struct v4l2_cropcap cropcap;
  struct v4l2_crop crop;
  struct v4l2_format fmt;
  unsigned int min;

  if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap))
  {
    if (EINVAL == errno)
    {
      fprintf(stderr, "%s is no V4L2 device\n", dev_name);
      exit(EXIT_FAILURE);
    }
    else
    {
      errno_exit("VIDIOC_QUERYCAP");
    }
  }

  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
  {
    fprintf(stderr, "%s is no video capture device\n", dev_name);
    exit(EXIT_FAILURE);
  }

  if (!(cap.capabilities & V4L2_CAP_STREAMING))
  {
    fprintf(stderr, "%s does not support streaming i/o\n", dev_name);
    exit(EXIT_FAILURE);
  }



  /* Select video input, video standard and tune here. */


  CLEAR(cropcap);

  cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap))
  {
    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    crop.c = cropcap.defrect;   /* reset to default */

    if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop))
    {
      switch (errno)
      {
      case EINVAL:
        /* Cropping not supported. */
        break;
      default:
        /* Errors ignored. */
        break;
      }
    }
  }
  else
  {
      /* Errors ignored. */
  }


  CLEAR(fmt);

  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = WIDTH;
  fmt.fmt.pix.height = HEIGHT;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
  fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

  if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
    errno_exit("VIDIOC_S_FMT");

  /* Note VIDIOC_S_FMT may change width and height. */

  /* Buggy driver paranoia. */
  min = fmt.fmt.pix.width * 2;
  if (fmt.fmt.pix.bytesperline < min)
    fmt.fmt.pix.bytesperline = min;
  min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
  if (fmt.fmt.pix.sizeimage < min)
    fmt.fmt.pix.sizeimage = min;

  if (WIDTH > fmt.fmt.pix.width)
    errno_exit("Width parameter is too big.\n");

  if (fmt.fmt.pix.height != HEIGHT)
    errno_exit("Height parameter is too big.\n");


  init_mmap();
}

static void close_device(void)
{
  if (-1 == close(fd))
    errno_exit("close");

  fd = -1;
}

static void open_device(void)
{
  struct stat st;

  if (-1 == stat(dev_name, &st))
  {
    fprintf(stderr, "Cannot identify '%s': %d, %s\n",
            dev_name, errno, strerror(errno));
    exit(EXIT_FAILURE);
  }

  if (!S_ISCHR(st.st_mode))
  {
    fprintf(stderr, "%s is no device\n", dev_name);
    exit(EXIT_FAILURE);
  }

  fd = open(dev_name, O_RDWR /* required */  | O_NONBLOCK, 0);

  if (-1 == fd)
  {
    fprintf(stderr, "Cannot open '%s': %d, %s\n",
            dev_name, errno, strerror(errno));
    exit(EXIT_FAILURE);
  }
}

static int sdl_filter(const SDL_Event * event)
{
  return (event->type == SDL_QUIT || event->type == SDL_KEYDOWN);
}

int main(int argc, char **argv)
{
  dev_name = "/dev/video0";

   setpriority(PRIO_PROCESS, 0, -10);
  generate_YCbCr_to_RGB_lookup();

  open_device();
  init_device();

  atexit(SDL_Quit);
  if (SDL_Init(SDL_INIT_VIDEO) < 0)
      return 1;

  SDL_WM_SetCaption(filter_names[filter_no], NULL);


  buffer_sdl = (uint8_t*)malloc(WIDTH*HEIGHT*3);

  SDL_SetVideoMode(WIDTH, HEIGHT, 32, SDL_DOUBLEBUF|SDL_ASYNCBLIT|SDL_HWACCEL|SDL_HWSURFACE);

  data_sf = SDL_CreateRGBSurfaceFrom(buffer_sdl, WIDTH, HEIGHT,
                                     24, WIDTH * 3,
                                     mask32(0), mask32(1), mask32(2), 0);

  SDL_SetEventFilter(sdl_filter);

  start_capturing();
  mainloop();
  stop_capturing();

  uninit_device();
  close_device();

  SDL_FreeSurface(data_sf);
  free(buffer_sdl);

  exit(EXIT_SUCCESS);

  return 0;
}
