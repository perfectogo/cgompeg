package main

import (
	"github.com/perfectogo/cgompeg/api"
)

func main() {

	// if len(os.Args) < 2 {
	// 	log.Fatalln(errors.New("not enough arguments"))
	// }

	// res := cmd.Cmd(os.Args[1], os.Args[2])

	// if res != 0 {
	// 	log.Fatalln(errors.New("HLS conversion failed"))
	// }

	api.Api()
}
