#include <iostream>
#include <cstdlib>
#include "fs_client.h"

using namespace std;

main(int argc, char *argv[])

{

    char *server;
    int server_port;
    unsigned int session, seq=0;
    char buf[10];

    if (argc != 3) {
        cout << "error: usage: " << argv[0] << " <server> <serverPort>\n";
        exit(1);
    }
    server = argv[1];
    server_port = atoi(argv[2]);

    fs_clientinit(server, server_port);


    fs_session("user1", "password1", &session, seq++);

    fs_create("user1", "password1", session, seq++, "tmp");
    fs_create("user1", "password1", session, seq++, "tmp1");
    fs_create("user1", "password1", session, seq++, "tmp2");
    fs_create("user1", "password1", session, seq++, "tmp13");
    fs_create("user1", "password1", session, seq++, "tmp4");
    fs_create("user1", "password1", session, seq++, "tmp15");
    fs_create("user1", "password1", session, seq++, "tmp6");
    fs_create("user1", "password1", session, seq++, "tmp17");
    fs_create("user1", "password1", session, seq++, "tmp8");
    fs_create("user1", "password1", session, seq++, "tmp19");
    fs_create("user1", "password1", session, seq++, "tmp10");
    fs_create("user1", "password1", session, seq++, "tmp11");
    fs_create("user1", "password1", session, seq++, "tmp12");
    fs_create("user1", "password1", session, seq++, "tmp113");
    fs_create("user1", "password1", session, seq++, "tmp14");
    fs_create("user1", "password1", session, seq++, "tmp115");
    fs_create("user1", "password1", session, seq++, "tmp16");
    fs_create("user1", "password1", session, seq++, "tmp117");
    fs_create("user1", "password1", session, seq++, "tmp18");
    fs_create("user1", "password1", session, seq++, "tmp119");
    fs_create("user1", "password1", session, seq++, "tmp20");
    fs_create("user1", "password1", session, seq++, "tmp211");
    fs_create("user1", "password1", session, seq++, "tmp22");
    fs_create("user1", "password1", session, seq++, "tmp123");
    fs_create("user1", "password1", session, seq++, "tmp24");
    fs_create("user1", "password1", session, seq++, "tmp241");
    fs_create("user1", "password1", session, seq++, "tmp456");
    fs_create("user1", "password1", session, seq++, "tmp1564");
    fs_create("user1", "password1", session, seq++, "tmp234");
    fs_create("user1", "password1", session, seq++, "tmp164");
    fs_create("user1", "password1", session, seq++, "tmp235");
    fs_create("user1", "password1", session, seq++, "tmp14664");
    fs_create("user1", "password1", session, seq++, "tmp1464");
    fs_create("user1", "password1", session, seq++, "tm466433");
	fs_create("user1", "password1", session, seq++, "tmp1464");
    fs_delete("user1", "password1", session, seq++, "tmp2");

	fs_delete("user1", "password1", session, seq++, "tmp2");



}
