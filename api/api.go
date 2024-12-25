package api

/*
#cgo LDFLAGS: -lavformat -lavcodec -lswscale -lavutil -ldl
#include <stdio.h>
#include <dlfcn.h>
#include "./../stream/cgompeg.h"
#include "./../stream/cgompeg.c"

int reload_sources() {
    // Close and reload the custom source files
    void *handle = dlopen("./../stream/cgompeg.so", RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        fprintf(stderr, "dlopen error: %s\n", dlerror());
        return -1;
    }
    return 0;
}
*/
import "C"

import (
	"io"
	"mime/multipart"
	"net/http"
	"unsafe"

	"github.com/labstack/echo/v4"
	"github.com/labstack/echo/v4/middleware"
	_ "github.com/perfectogo/cgompeg/api/docs"
	echoSwagger "github.com/swaggo/echo-swagger"
)

func InputStream(inputFile *multipart.FileHeader) int {
	// Open the file
	file, err := inputFile.Open()
	if err != nil {
		panic("Failed to open multipart file: " + err.Error())
	}
	defer file.Close()

	// Read the file content into a buffer
	data, err := io.ReadAll(file)
	if err != nil {
		panic("Failed to read file content: " + err.Error())
	}

	// Pass the file content to C
	result := C.inputStream((*C.uint8_t)(unsafe.Pointer(&data[0])), C.size_t(len(data)))

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

	// Pass the temporary file to the C function
	if result := InputStream(file); result != 0 {
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
