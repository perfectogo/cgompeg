package api

/*
#cgo LDFLAGS: -lavformat -lavcodec -lswscale -lavutil
#include <stdio.h>
#include "./../stream/cgompeg.h"
#include "./../stream/cgompeg.c"
*/
import "C"

import (
	"fmt"
	"io"
	"net/http"
	"os"

	"github.com/labstack/echo/v4"
	"github.com/labstack/echo/v4/middleware"
	echoSwagger "github.com/swaggo/echo-swagger"
)

func InputStream(inputFile *os.File) int {
	// Seek to beginning of file before reading
	inputFile.Seek(0, 0)

	cInputFile := C.fdopen(C.int(inputFile.Fd()), C.CString("rb"))
	if cInputFile == nil {
		return -1
	}

	result := C.inputStream(cInputFile)

	fmt.Println("result", result)
	// Let C close the file instead of Go
	// Don't use defer here as it can cause double free
	C.fclose(cInputFile)

	return int(result)
}

// @Summary Upload a file
// @Description Upload a file to be processed
// @Accept multipart/form-data
// @Produce application/json
// @Param file formData file true "File to upload"
// @Success 200 {string} string "File processed successfully! Output saved to a file"
// @Failure 400 {string} string "Failed to retrieve file"
// @Failure 500 {string} string "Failed to open file"
// @Failure 500 {string} string "Failed to create temporary file"
// @Router /upload [post]
func uploadHandler(c echo.Context) error {
	// Retrieve the file from the form
	file, err := c.FormFile("file")
	{
		if err != nil {
			return c.String(http.StatusBadRequest, "Failed to retrieve file: "+err.Error())
		}
	}

	// Open the uploaded file
	src, err := file.Open()
	{
		if err != nil {
			return c.String(http.StatusInternalServerError, "Failed to open file: "+err.Error())
		}
	}
	defer src.Close()

	// Create a Go temporary file to act as a stream
	tempFile, err := os.CreateTemp("", "upload-*")
	{
		if err != nil {
			return c.String(http.StatusInternalServerError, "Failed to create temporary file: "+err.Error())
		}
		defer os.Remove(tempFile.Name())
		defer tempFile.Close()
	}
	// Copy the uploaded file to the temporary file
	if _, err = io.Copy(tempFile, src); err != nil {
		return c.String(http.StatusInternalServerError, "Failed to save file: "+err.Error())
	}

	// Pass the temporary file to the C function
	if result := InputStream(tempFile); result != 0 {
		return c.String(http.StatusInternalServerError, "Failed to process file.")
	}

	return c.String(http.StatusOK, "File processed successfully! Output saved to a file")
}

// @title Cgompeg API
// @version 1.0
// @description This is a sample server for Cgompeg API.
// @host localhost:8080
// @BasePath /
func Api() {
	e := echo.New()

	// cors
	e.Use(middleware.CORSWithConfig(middleware.CORSConfig{
		AllowOrigins: []string{"*"},
		AllowMethods: []string{http.MethodGet, http.MethodPost, http.MethodPut, http.MethodDelete},
	}))

	// Route for file upload
	e.POST("/upload", uploadHandler)
	// swagger
	e.GET("/swagger/*", echoSwagger.WrapHandler)

	// Start the server
	e.Logger.Fatal(e.Start(":8080"))
}
