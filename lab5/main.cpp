#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include "check.hpp"

using namespace std;

// Split a string by a delimiter
vector<string> split(const string& str, char delimiter) {
    vector<string> result;
    string temp;
    for (char ch : str) {
        if (ch == delimiter) {
            result.push_back(temp);
            temp.clear();
        } else {
            temp += ch;
        }
    }
    result.push_back(temp);
    return result;
}

// Read from a file descriptor and return lines
vector<string> read_file(int fd) {
    off_t file_size = check(lseek(fd, 0, SEEK_END));
    check(lseek(fd, 0, SEEK_SET));

    vector<char> buffer(file_size + 1);
    ssize_t bytes_read = check(read(fd, buffer.data(), file_size));
    if (bytes_read != file_size) {
        perror("read");
        close(fd);
        exit(1);
    }

    buffer[file_size] = '\0';
    close(fd);

    string content(buffer.data());
    return split(content, '\n');
}

// Get the password hash for a user from /etc/shadow
string get_password_hash(const string& username, const vector<string>& shadow_lines) {
    for (const auto& line : shadow_lines) {
        auto parts = split(line, ':');
        if (parts.size() >= 2 && parts[0] == username) {
            return parts[1];
        }
    }
    return "[NO ACCESS]";
}

// Get the groups a user belongs to from /etc/group
vector<string> get_user_groups(const string& username, const vector<string>& group_lines) {
    vector<string> result;
    for (const auto& line : group_lines) {
        auto fields = split(line, ':');
        if (fields.size() >= 4) {
            auto members = split(fields[3], ',');
            for (auto m : members) {
                string member = m;
                while (!member.empty() && isspace(member.front())) member.erase(member.begin());
                while (!member.empty() && isspace(member.back())) member.pop_back();

                if (member == username) {
                    result.push_back(fields[0]);
                    break;
                }
            }
        }
    }
    return result;
}

// Get the groups where the user is an admin from /etc/gshadow
vector<string> get_admin_groups(const string& username, const vector<string>& gshadow_lines) {
    vector<string> result;
    for (const auto& line : gshadow_lines) {
        auto parts = split(line, ':');
        if (parts.size() >= 3) {
            auto admins = split(parts[2], ',');
            for (auto admin : admins) {
                while (!admin.empty() && isspace(admin.back())) admin.pop_back();
                while (!admin.empty() && isspace(admin.front())) admin.erase(admin.begin());

                if (admin == username) {
                    result.push_back(parts[0]);  // group name
                    break;
                }
            }
        }
    }
    return result;
}

int main() {
    uid_t original_uid = getuid();

    int fd_shadow = check(open("/etc/shadow", O_RDONLY));
    int fd_gshadow= check(open("/etc/gshadow", O_RDONLY));

    check(setuid(original_uid));

    int fd_passwd = check(open("/etc/passwd", O_RDONLY));
    int fd_group  = check(open("/etc/group",  O_RDONLY));

    auto shadow_lines = read_file(fd_shadow);
    auto gshadow_lines = read_file(fd_gshadow);
    auto passwd_lines = read_file(fd_passwd);
    auto group_lines  = read_file(fd_group);

    for (const auto& line : passwd_lines) {
        auto fields = split(line, ':');
        if (fields.size() < 7) continue;

        string username = fields[0];
        string uid      = fields[2];
        string home     = fields[5];

        cout << "Username: " << username << endl;
        cout << " UID: " << uid << endl;
        cout << " Home Directory: " << home << endl;

        string hash = get_password_hash(username, shadow_lines);
        cout << " Password Hash: " << hash << endl;

        auto groups = get_user_groups(username,group_lines);
        cout << " Groups: ";
        for (size_t i = 0; i < groups.size(); ++i) {
            cout << groups[i];
            if (i + 1 != groups.size()) cout << ", ";
        }
        cout << endl;

        auto admin_groups = get_admin_groups(username, gshadow_lines);
        cout << " Admin in Groups: ";
        for (size_t i = 0; i < admin_groups.size(); ++i) {
            cout << admin_groups[i];
            if (i + 1 != admin_groups.size()) cout << ", ";
        }
        cout << endl << endl;
    }

    return 0;
}