package api

/*
#cgo LDFLAGS: -lavformat -lavcodec -lswscale -lavutil
#include "./stream/cgompeg.h"
#include "./stream/cgompeg.c"
*/
import "C"
import (
	"encoding/binary"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"sync"

	"github.com/labstack/echo/v4"
	echoSwagger "github.com/swaggo/echo-swagger"

	_ "github.com/perfectogo/cgompeg/api/docs"
)

type Metadata struct {
	FileSize   int64
	Duration   float64
	Bitrate    float64
	Width      int32
	Height     int32
	FrameRate  float64
	MimeType   [128]byte
	Extension  [16]byte
	Resolution [32]byte
}

// @title Go and C Code Interoperability API
// @version 1.0
// @description This is a sample API for Go and C code interoperability.
// @host localhost:8080
// @BasePath /
func Api() {
	e := echo.New()
	e.POST("/upload", upload)
	e.GET("/swagger/*", echoSwagger.WrapHandler)
	e.Start(":8080")
}

// upload is the handler for the upload endpoint
// it receives a file from the client and writes it to a pipe
// the C function then reads the file from the pipe and writes it to a file

// godoc upload
// @Summary Upload a file
// @Description Upload a file to the server
// @Accept multipart/form-data
// @Param file formData file true "The file to upload"
// @Success 200 {string} string "File processed and saved by C!"
// @Failure 400 {string} string "Failed to retrieve file"
// @Failure 500 {string} string "Failed to open uploaded file"
// @Failure 500 {string} string "Failed to create pipe"
// @Failure 500 {string} string "Failed to read from pipe"
// @Router /upload [post]
func upload(c echo.Context) error {
	file, err := c.FormFile("file")
	if err != nil {
		return c.String(http.StatusBadRequest, "Failed to get file")
	}

	src, err := file.Open()
	if err != nil {
		return c.String(http.StatusInternalServerError, "Failed to open file")
	}
	defer src.Close()

	rPipe, wPipe, err := os.Pipe()
	if err != nil {
		return c.String(http.StatusInternalServerError, "Failed to create pipe")
	}

	var wg sync.WaitGroup
	wg.Add(1)

	// Create a channel for error handling
	errChan := make(chan error, 1)

	// Create metadata
	var header Metadata
	{
		header.FileSize = file.Size
		copy(header.MimeType[:], file.Header.Get("Content-Type"))
		copy(header.Extension[:], filepath.Ext(file.Filename))
		copy(header.Resolution[:], file.Header.Get("Content-Resolution"))
		header.Duration = 0
		header.Bitrate = 0
		header.Width = 0
		header.Height = 0
		header.FrameRate = 0
	}

	go func() {
		defer wg.Done()
		defer wPipe.Close()

		// Write metadata first
		if err := binary.Write(wPipe, binary.LittleEndian, header); err != nil {
			errChan <- err
			return
		}

		// Then write file content
		if _, err := io.Copy(wPipe, src); err != nil {
			errChan <- err
			return
		}
	}()

	// Process the data in C
	result := C.read_pipe(C.int(rPipe.Fd()))
	rPipe.Close()

	// Wait for writing to complete
	wg.Wait()

	// Check for any errors from the goroutine
	select {
	case err := <-errChan:
		return c.String(http.StatusInternalServerError, "Failed to write data: "+err.Error())
	default:
		if result != 0 {
			return c.String(http.StatusInternalServerError, "Failed to process video")
		}
	}

	return c.String(http.StatusOK, "Stream processed successfully!")
}
