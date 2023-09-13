package main

import (
	"fmt"
	"net"

	u "github.com/cbot918/liby/util"
)

type Wsy struct {
	listener net.Listener
}

func New(port string) *Wsy {
	w := new(Wsy)
	l, err := net.Listen("tcp", "localhost:"+port)
	u.Checke(err, "net listen failed")
	fmt.Println("[*] ws-server listening: " + port)
	w.listener = l
	return w
}

func (w *Wsy) Run() string {

	for {
		conn, err := w.listener.Accept()
		u.Checke(err, "listener accept error")

		go func(c net.Conn) {
			defer c.Close()

			ch := NewConnHandler(conn)

			req := ch.ReadSocket() // 讀取web ws發過來的第一個http request

			err := ch.Upgrade(string(req)) // 將連線升級成 websocket
			u.Checke(err, "upgrade write socket failed")
			fmt.Println("[*] ws upgrade success!")

			res := ch.ReadSocket() // 寫死: 預設web會發一個message過來
			fmt.Printf("[*] receved message\n\n")

			message := ch.DecodeFrame(res) // 根據Spec解碼Frame把message取出來
			var Green = "\033[32m"
			var Reset = "\033[0m"
			fmt.Printf("\nclient: %s%s%s\n", Green, string(message), Reset)

			fmt.Fprint(c, "hihi")

		}(conn)
	}
}
