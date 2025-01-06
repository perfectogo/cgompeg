package api

/*
#cgo LDFLAGS: -lavformat -lavcodec -lswscale -lavutil -pthread
#include <string.h>
#include "./stream/cgompeg.h"
#include "./stream/cgompeg.c"
*/
import "C"
import (
	"io"
	"net/http"
	"os"

	"github.com/labstack/echo/v4"
	"github.com/labstack/echo/v4/middleware"
	echoSwagger "github.com/swaggo/echo-swagger"

	_ "github.com/perfectogo/cgompeg/api/docs"
)

// Metadata represents the video file metadata that will be passed to C
type Metadata struct {
	FileSize   int64     // File size in bytes
	MimeType   [128]byte // MIME type of the video
	Extension  [16]byte  // File extension
	Resolution [32]byte  // Video resolution string
}

// NewServer creates and configures the Echo server
func NewServer() *echo.Echo {
	e := echo.New()

	// Middleware
	e.Use(middleware.Logger())
	e.Use(middleware.Recover())
	e.Use(middleware.CORS())

	// Routes
	e.POST("/upload", handleUpload)
	e.GET("/swagger/*", echoSwagger.WrapHandler)

	return e
}

// handleUpload handles the file upload and HLS conversion
// @Summary Upload video file for HLS conversion
// @Description Upload a video file to convert it to HLS format
// @Accept multipart/form-data
// @Produce json
// @Param file formData file true "Video file to convert"
// @Success 200 {object} map[string]string "Successfully converted to HLS"
// @Failure 400 {object} map[string]string "Bad request"
// @Failure 500 {object} map[string]string "Internal server error"
// @Router /upload [post]
func handleUpload(c echo.Context) error {

	file, err := c.FormFile("file")
	{
		if err != nil {
			return c.JSON(http.StatusBadRequest, map[string]string{
				"error": "No file uploaded",
			})
		}
	}

	// Open the uploaded file
	src, err := file.Open()
	{
		if err != nil {
			return c.JSON(http.StatusInternalServerError, map[string]string{
				"error": "Failed to open uploaded file",
			})
		}

		defer src.Close()
	}

	// Create pipe for communication with C code
	rPipe, wPipe, err := os.Pipe()
	{
		if err != nil {
			return c.JSON(http.StatusInternalServerError, map[string]string{
				"error": "Failed to create pipe",
			})
		}
	}

	// var metadata Metadata
	// {
	// 	metadata.FileSize = file.Size
	// 	copy(metadata.MimeType[:], file.Header.Get("Content-Type"))
	// 	copy(metadata.Extension[:], filepath.Ext(file.Filename))
	// 	copy(metadata.Resolution[:], file.Header.Get("Content-Resolution"))
	// }

	// cMetadata := C.MetaData{}
	// {
	// 	// Replace the direct copy with unsafe pointer copy
	// 	C.memcpy(unsafe.Pointer(&cMetadata.FileSize), unsafe.Pointer(&metadata.FileSize), 8)
	// 	C.memcpy(unsafe.Pointer(&cMetadata.MimeType[0]), unsafe.Pointer(&metadata.MimeType[0]), 128)
	// 	C.memcpy(unsafe.Pointer(&cMetadata.Extension[0]), unsafe.Pointer(&metadata.Extension[0]), 16)
	// 	C.memcpy(unsafe.Pointer(&cMetadata.Resolution[0]), unsafe.Pointer(&metadata.Resolution[0]), 32)
	// }

	go func() {

		if _, err := io.Copy(wPipe, src); err != nil {
			return
		}
		wPipe.Close()
	}()

	// Process the data in C
	result := C.read_pipe(C.int(rPipe.Fd()), &C.MetaData{})
	{
		if int(result) != 0 {
			return c.JSON(http.StatusInternalServerError, map[string]string{
				"error": "Failed to process video",
			})
		}
	}

	// Close read pipe after successful processing
	defer rPipe.Close()

	return c.JSON(http.StatusOK, map[string]string{
		"message": "Video successfully converted to HLS",
		"status":  "success",
	})
}

// StartServer starts the HTTP server
func StartServer(address string) error {
	e := NewServer()
	return e.Start(address)
}
