func upload(c echo.Context) error {
    file, err := c.FormFile("file")
    if err != nil {
        return c.String(http.StatusBadRequest, "Failed to retrieve file: "+err.Error())
    }

    // Get file metadata
    src, err := file.Open()
    if err != nil {
        return c.String(http.StatusInternalServerError, "Failed to open file: "+err.Error())
    }
    defer src.Close()

    // Create a buffer to store file metadata
    var header struct {
        FileSize   int64
        MimeType   string
        Extension  string
        VideoInfo  struct {
            Width    int
            Height   int
            Duration float64
        }
    }

    // Fill metadata
    header.FileSize = file.Size
    header.MimeType = file.Header.Get("Content-Type")
    header.Extension = filepath.Ext(file.Filename)

    // Create pipes for data transfer
    rPipe, wPipe, err := os.Pipe()
    if err != nil {
        return c.String(http.StatusInternalServerError, "Failed to create pipe")
    }

    go func() {
        defer wPipe.Close()
        // Write metadata first
        binary.Write(wPipe, binary.LittleEndian, header)
        // Then write file content
        io.Copy(wPipe, src)
    }()

    code := C.cmd_from_pipe(C.int(rPipe.Fd()))
    if code != 0 {
        return c.String(http.StatusInternalServerError, "Failed to process video")
    }

    rPipe.Close()
    return c.String(http.StatusOK, "File processed successfully!")
}