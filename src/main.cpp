#include "mlms/core/errors.hpp"
#include "mlms/domain/role.hpp"
#include "mlms/domain/user_record.hpp"
#include "mlms/persistence/schema_initializer.hpp"
#include "mlms/persistence/sqlite_connection.hpp"
#include "mlms/persistence/user_repository.hpp"
#include "mlms/persistence/department_repository.hpp"
#include "mlms/persistence/course_repository.hpp"
#include "mlms/security/sodium_password_hasher.hpp"
#include "mlms/services/authentication_service.hpp"
#include "mlms/services/authorization_guard.hpp"
#include "mlms/persistence/enrollment_repository.hpp"
#include "mlms/persistence/assignment_repository.hpp"
#include "mlms/persistence/submission_repository.hpp"
#include "mlms/services/late_penalty.hpp"
#include "mlms/persistence/mark_repository.hpp"
#include "mlms/persistence/audit_repository.hpp"
#include "mlms/persistence/notification_repository.hpp"
#include "mlms/domain/submission.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <vector>
#include <optional>

namespace {

std::string readPassword(const std::string& prompt) {
    std::cout << prompt << std::flush;

    termios original{};
    if (tcgetattr(STDIN_FILENO, &original) != 0) {
        std::string password;
        if (!std::getline(std::cin, password))
            throw std::runtime_error("Password input interrupted.");
        return password;
    }

    termios hidden = original;
    hidden.c_lflag &= ~ECHO;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &hidden) != 0) {
        std::string password;
        if (!std::getline(std::cin, password))
            throw std::runtime_error("Password input interrupted.");
        return password;
    }

    std::string password;
    bool ok = static_cast<bool>(std::getline(std::cin, password));

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
    std::cout << '\n';

    if (!ok)
        throw std::runtime_error("Password input interrupted.");

    return password;
}

std::string readRequired(const std::string& prompt) {
    while (true) {
        std::cout << prompt;

        std::string value;
        if (!std::getline(std::cin, value))
            throw std::runtime_error("Input interrupted.");

        if (value.find_first_not_of(" \t\r\n") != std::string::npos)
            return value;

        std::cout << "This field cannot be empty.\n";
    }
}

std::string readOptional(const std::string& prompt) {
    std::cout << prompt;
    std::string value;

    if (!std::getline(std::cin, value))
        throw std::runtime_error("Input interrupted.");

    return value;
}

int readPositiveInt(const std::string& prompt) {
    while (true) {
        std::string value = readRequired(prompt);

        try {
            std::size_t used = 0;
            int number = std::stoi(value, &used);

            if (used == value.size() && number > 0)
                return number;
        } catch (const std::exception&) {
            // Show the validation message below.
        }

        std::cout << "Enter a valid positive integer.\n";
    }
}

bool validEmail(const std::string& email) {
    const auto at = email.find('@');
    return at != std::string::npos
        && at > 0
        && at < email.size() - 1
        && email.size() <= 254;
}

mlms::Role readRole() {
    while (true) {
        std::cout
            << "Select role:\n"
            << "1. Student\n"
            << "2. Lecturer\n"
            << "3. HoD\n"
            << "4. Administrator\n";

        const std::string choice = readRequired("Role: ");

        if (choice == "1") return mlms::Role::Student;
        if (choice == "2") return mlms::Role::Lecturer;
        if (choice == "3") return mlms::Role::HoD;
        if (choice == "4") return mlms::Role::Administrator;

        std::cout << "Invalid role selection.\n";
    }
}

void createFirstAdministrator(
    mlms::UserRepository& users,
    const mlms::SodiumPasswordHasher& hasher
) {
    std::cout
        << "\nNo users exist in the database.\n"
        << "Create the first administrator account.\n"
        << "This setup is available only while the user table is empty.\n\n";

    mlms::UserRecord admin;
    admin.username = readRequired("Administrator username: ");
    admin.fullName = readRequired("Full name: ");

    do {
        admin.email = readRequired("Email address: ");
        if (!validEmail(admin.email))
            std::cout << "Enter a valid email address.\n";
    } while (!validEmail(admin.email));

    while (true) {
        const std::string password =
            readPassword("Create password (minimum 12 characters): ");

        if (password.size() < 12) {
            std::cout << "Password must contain at least 12 characters.\n";
            continue;
        }

        if (password != readPassword("Confirm password: ")) {
            std::cout << "Passwords do not match. Try again.\n";
            continue;
        }

        admin.passwordHash = hasher.hashPassword(password);
        break;
    }

    admin.role = mlms::Role::Administrator;
    users.create(admin);

    std::cout << "\nAdministrator account created successfully.\n"
              << "You can now log in.\n";
}

void listUsers(mlms::UserRepository& users) {
    const auto records = users.listAll();

    if (records.empty()) {
        std::cout << "No users found.\n";
        return;
    }

    std::cout << "\n--- Registered Users ---\n";

    for (const auto& user : records) {
        std::cout
            << "ID: " << user.userId
            << " | Username: " << user.username
            << " | Name: " << user.fullName
            << " | Email: " << user.email
            << " | Role: " << mlms::toString(user.role)
            << " | Status: " << (user.active ? "Active" : "Inactive")
            << '\n';
    }
}

void searchUser(mlms::UserRepository& users) {
    const std::string key = readRequired("Enter user ID or username: ");

    std::optional<mlms::UserRecord> result;

    const bool numeric = !key.empty()
        && std::all_of(key.begin(), key.end(),
            [](unsigned char ch) { return std::isdigit(ch) != 0; });

    if (numeric) {
        try {
            result = users.findById(std::stoi(key));
        } catch (const std::exception&) {
            result = std::nullopt;
        }
    } else {
        result = users.findByUsername(key);
    }

    if (!result) {
        std::cout << "User not found.\n";
        return;
    }

    const auto& user = *result;

    std::cout
        << "\nUser ID: " << user.userId
        << "\nUsername: " << user.username
        << "\nFull name: " << user.fullName
        << "\nEmail: " << user.email
        << "\nRole: " << mlms::toString(user.role)
        << "\nStatus: " << (user.active ? "Active" : "Inactive")
        << "\nStudent number: "
        << user.studentNumber.value_or("N/A")
        << "\nStaff number: "
        << user.staffNumber.value_or("N/A")
        << "\nDepartment ID: ";

    if (user.departmentId)
        std::cout << *user.departmentId;
    else
        std::cout << "N/A";

    std::cout << '\n';
}

void registerUser(
    mlms::UserRepository& users,
    const mlms::SodiumPasswordHasher& hasher
) {
    mlms::UserRecord user;

    user.username = readRequired("Username (3-50 characters): ");
    if (user.username.size() < 3 || user.username.size() > 50) {
        std::cout << "Username must contain 3-50 characters.\n";
        return;
    }

    if (users.existsByUsername(user.username)) {
        std::cout << "That username is already registered.\n";
        return;
    }

    user.fullName = readRequired("Full name: ");
    if (user.fullName.size() > 120) {
        std::cout << "Full name cannot exceed 120 characters.\n";
        return;
    }

    user.email = readRequired("Email address: ");
    if (!validEmail(user.email)) {
        std::cout << "Enter a valid email address.\n";
        return;
    }

    user.role = readRole();

    if (user.role == mlms::Role::Student) {
        user.studentNumber = readRequired("Student number: ");
    } else {
        user.staffNumber = readRequired("Staff number: ");
    }

    std::string password;

    while (true) {
        password = readPassword("Password (minimum 12 characters): ");

        if (password.size() < 12) {
            std::cout << "Password must contain at least 12 characters.\n";
            continue;
        }

        if (password != readPassword("Confirm password: ")) {
            std::cout << "Passwords do not match. Try again.\n";
            continue;
        }

        break;
    }

    user.passwordHash = hasher.hashPassword(password);
    user.active = true;

    try {
        const int id = users.create(user);
        std::cout << "User registered successfully. User ID: "
                  << id << '\n';
    } catch (const std::exception& error) {
        std::cout << "Could not register user: "
                  << error.what() << '\n';
    }
}

void updateUser(mlms::UserRepository& users) {
    const int id = readPositiveInt("Enter user ID to update: ");
    const auto user = users.findById(id);

    if (!user) {
        std::cout << "User not found.\n";
        return;
    }

    std::cout << "Current name: " << user->fullName << '\n';
    std::string name = readRequired("New full name: ");

    if (name.size() > 120) {
        std::cout << "Full name cannot exceed 120 characters.\n";
        return;
    }

    std::string email = readRequired("New email address: ");

    if (!validEmail(email)) {
        std::cout << "Enter a valid email address.\n";
        return;
    }

    if (users.updateProfile(id, name, email))
        std::cout << "User profile updated successfully.\n";
    else
        std::cout << "No changes were saved.\n";
}

void changeUserStatus(
    mlms::AuthenticatedUser const& currentUser,
    mlms::UserRepository& users
) {
    const int id = readPositiveInt("Enter user ID: ");
    const auto target = users.findById(id);

    if (!target) {
        std::cout << "User not found.\n";
        return;
    }

    if (target->active && target->userId == currentUser.userId) {
        std::cout << "You cannot deactivate your own account while logged in.\n";
        return;
    }

    const bool newStatus = !target->active;

    if (!newStatus && target->role == mlms::Role::Administrator) {
        const auto allUsers = users.listAll();

        const auto activeAdmins = std::count_if(
            allUsers.begin(), allUsers.end(),
            [](const mlms::UserRecord& user) {
                return user.role == mlms::Role::Administrator
                    && user.active;
            });

        if (activeAdmins <= 1) {
            std::cout << "Cannot deactivate the last active administrator.\n";
            return;
        }
    }

    std::cout << "Account currently "
              << (target->active ? "active" : "inactive") << ".\n";

    if (users.setActive(id, newStatus))
        std::cout << "Account is now "
                  << (newStatus ? "active" : "inactive") << ".\n";
    else
        std::cout << "Account status was not changed.\n";
}

void manageUsers(
    const mlms::AuthenticatedUser& currentUser,
    mlms::UserRepository& users,
    const mlms::SodiumPasswordHasher& hasher
) {
    mlms::AuthorizationGuard::requireAdministrator(currentUser.role);

    while (true) {
        std::cout
            << "\n========== USER MANAGEMENT ==========\n"
            << "1. List all users\n"
            << "2. Search user\n"
            << "3. Register user\n"
            << "4. Update user profile\n"
            << "5. Activate/deactivate account\n"
            << "0. Return to dashboard\n"
            << "Select an option: ";

        std::string choice;
        if (!std::getline(std::cin, choice))
            return;

        try {
            if (choice == "1") {
                listUsers(users);
            } else if (choice == "2") {
                searchUser(users);
            } else if (choice == "3") {
                registerUser(users, hasher);
            } else if (choice == "4") {
                updateUser(users);
            } else if (choice == "5") {
                changeUserStatus(currentUser, users);
            } else if (choice == "0") {
                return;
            } else {
                std::cout << "Invalid option. Try again.\n";
            }
        } catch (const std::exception& error) {
            std::cout << "Operation failed: " << error.what() << '\n';
        }
    }
}


void listDepartments(mlms::DepartmentRepository& departments) {
    const auto records = departments.listAll();

    if (records.empty()) {
        std::cout << "No departments found.\n";
        return;
    }

    std::cout << "\n--- Departments ---\n";

    for (const auto& department : records) {
        std::cout
            << "ID: " << department.departmentId
            << " | Name: " << department.departmentName
            << " | HoD ID: ";

        if (department.hodId)
            std::cout << *department.hodId;
        else
            std::cout << "Not assigned";

        std::cout << '\n';
    }
}

void createDepartment(mlms::DepartmentRepository& departments) {
    const std::string name = readRequired("Department name: ");

    try {
        const int id = departments.create(name);
        std::cout << "Department created successfully. ID: "
                  << id << '\n';
    } catch (const std::exception& error) {
        std::cout << "Could not create department: "
                  << error.what() << '\n';
    }
}

void searchDepartment(mlms::DepartmentRepository& departments) {
    const std::string key = readRequired("Enter department ID or name: ");

    std::optional<mlms::DepartmentRecord> result;

    const bool numeric = std::all_of(
        key.begin(), key.end(),
        [](unsigned char ch) { return std::isdigit(ch) != 0; });

    if (numeric) {
        try {
            result = departments.findById(std::stoi(key));
        } catch (const std::exception&) {
            result = std::nullopt;
        }
    } else {
        result = departments.findByName(key);
    }

    if (!result) {
        std::cout << "Department not found.\n";
        return;
    }

    std::cout << "\nDepartment ID: " << result->departmentId
              << "\nDepartment name: " << result->departmentName
              << "\nHoD ID: ";

    if (result->hodId)
        std::cout << *result->hodId;
    else
        std::cout << "Not assigned";

    std::cout << '\n';
}

void renameDepartment(mlms::DepartmentRepository& departments) {
    const int id = readPositiveInt("Enter department ID: ");

    if (!departments.findById(id)) {
        std::cout << "Department not found.\n";
        return;
    }

    const std::string name = readRequired("New department name: ");

    try {
        if (departments.updateName(id, name))
            std::cout << "Department name updated successfully.\n";
        else
            std::cout << "Department name was not changed.\n";
    } catch (const std::exception& error) {
        std::cout << "Could not update department: "
                  << error.what() << '\n';
    }
}

void assignDepartmentHead(
    mlms::DepartmentRepository& departments,
    mlms::UserRepository& users
) {
    const int departmentId = readPositiveInt("Enter department ID: ");

    if (!departments.findById(departmentId)) {
        std::cout << "Department not found.\n";
        return;
    }

    const int hodId = readPositiveInt("Enter HoD user ID: ");
    const auto user = users.findById(hodId);

    if (!user) {
        std::cout << "User not found.\n";
        return;
    }

    if (user->role != mlms::Role::HoD) {
        std::cout << "The selected user does not have the HoD role.\n";
        return;
    }

    if (!user->active) {
        std::cout << "Cannot assign an inactive user as department head.\n";
        return;
    }

    try {
        if (departments.assignHead(departmentId, hodId))
            std::cout << "Department head assigned successfully.\n";
        else
            std::cout << "Department head was not assigned.\n";
    } catch (const std::exception& error) {
        std::cout << "Could not assign department head: "
                  << error.what() << '\n';
    }
}

void manageDepartments(
    const mlms::AuthenticatedUser& currentUser,
    mlms::DepartmentRepository& departments,
    mlms::UserRepository& users
) {
    mlms::AuthorizationGuard::requireAdministrator(currentUser.role);

    while (true) {
        std::cout
            << "\n========== DEPARTMENT MANAGEMENT ==========\n"
            << "1. Create department\n"
            << "2. List departments\n"
            << "3. Search department\n"
            << "4. Update department name\n"
            << "5. Assign Head of Department\n"
            << "0. Return to dashboard\n"
            << "Select an option: ";

        std::string choice;
        if (!std::getline(std::cin, choice))
            return;

        if (choice == "1") {
            createDepartment(departments);
        } else if (choice == "2") {
            listDepartments(departments);
        } else if (choice == "3") {
            searchDepartment(departments);
        } else if (choice == "4") {
            renameDepartment(departments);
        } else if (choice == "5") {
            assignDepartmentHead(departments, users);
        } else if (choice == "0") {
            return;
        } else {
            std::cout << "Invalid option. Try again.\n";
        }
    }
}

void listCourses(mlms::CourseRepository& courses) {
    const auto records = courses.listAll();

    if (records.empty()) {
        std::cout << "No courses found.\n";
        return;
    }

    std::cout << "\n--- Courses ---\n";

    for (const auto& course : records) {
        std::cout
            << "ID: " << course.courseId
            << " | Code: " << course.courseCode
            << " | Title: " << course.courseTitle
            << " | Department ID: " << course.departmentId
            << " | Lecturer ID: ";

        if (course.lecturerId)
            std::cout << *course.lecturerId;
        else
            std::cout << "Not assigned";

        std::cout << '\n';
    }
}

void createCourse(
    mlms::CourseRepository& courses,
    mlms::DepartmentRepository& departments
) {
    const std::string code = readRequired("Course code: ");
    const std::string title = readRequired("Course title: ");
    const int departmentId = readPositiveInt("Department ID: ");

    if (!departments.findById(departmentId)) {
        std::cout << "Department not found. Course was not created.\n";
        return;
    }

    try {
        const int id = courses.create(code, title, departmentId);

        std::cout << "Course created successfully. ID: "
                  << id << '\n';
    } catch (const std::exception& error) {
        std::cout << "Could not create course: "
                  << error.what() << '\n';
    }
}

void searchCourse(mlms::CourseRepository& courses) {
    const std::string key = readRequired("Enter course ID or code: ");

    std::optional<mlms::CourseRecord> result;

    const bool numeric = std::all_of(
        key.begin(),
        key.end(),
        [](unsigned char ch) {
            return std::isdigit(ch) != 0;
        }
    );

    if (numeric) {
        try {
            result = courses.findById(std::stoi(key));
        } catch (const std::exception&) {
            result = std::nullopt;
        }
    } else {
        result = courses.findByCode(key);
    }

    if (!result) {
        std::cout << "Course not found.\n";
        return;
    }

    std::cout
        << "\nCourse ID: " << result->courseId
        << "\nCourse code: " << result->courseCode
        << "\nCourse title: " << result->courseTitle
        << "\nDepartment ID: " << result->departmentId
        << "\nLecturer ID: ";

    if (result->lecturerId)
        std::cout << *result->lecturerId;
    else
        std::cout << "Not assigned";

    std::cout << '\n';
}

void updateCourse(mlms::CourseRepository& courses) {
    const int id = readPositiveInt("Course ID to update: ");

    if (!courses.findById(id)) {
        std::cout << "Course not found.\n";
        return;
    }

    const std::string code = readRequired("New course code: ");
    const std::string title = readRequired("New course title: ");

    try {
        if (courses.updateDetails(id, code, title))
            std::cout << "Course details updated successfully.\n";
        else
            std::cout << "Course details were not changed.\n";
    } catch (const std::exception& error) {
        std::cout << "Could not update course: "
                  << error.what() << '\n';
    }
}

void assignCourseLecturer(
    mlms::CourseRepository& courses,
    mlms::UserRepository& users
) {
    const int courseId = readPositiveInt("Course ID: ");

    if (!courses.findById(courseId)) {
        std::cout << "Course not found.\n";
        return;
    }

    std::cout
        << "Enter lecturer user ID, or 0 to unassign: ";

    std::string input;
    if (!std::getline(std::cin, input))
        return;

    try {
        std::size_t used = 0;
        const int lecturerId = std::stoi(input, &used);

        if (used != input.size() || lecturerId < 0) {
            std::cout << "Enter a valid user ID or 0.\n";
            return;
        }

        std::optional<int> assignedLecturer;

        if (lecturerId != 0) {
            const auto lecturer = users.findById(lecturerId);

            if (!lecturer) {
                std::cout << "User not found.\n";
                return;
            }

            if (lecturer->role != mlms::Role::Lecturer) {
                std::cout << "The selected user is not a Lecturer.\n";
                return;
            }

            if (!lecturer->active) {
                std::cout << "Cannot assign an inactive lecturer.\n";
                return;
            }

            assignedLecturer = lecturerId;
        }

        if (courses.assignLecturer(courseId, assignedLecturer)) {
            if (assignedLecturer)
                std::cout << "Lecturer assigned successfully.\n";
            else
                std::cout << "Lecturer unassigned successfully.\n";
        } else {
            std::cout << "Lecturer assignment was not changed.\n";
        }
    } catch (const std::exception& error) {
        std::cout << "Invalid input or assignment failed: "
                  << error.what() << '\n';
    }
}


void enrollStudent(
    mlms::EnrollmentRepository& enrollments,
    mlms::UserRepository& users,
    mlms::CourseRepository& courses
) {
    const int studentId = readPositiveInt("Student user ID: ");

    const auto student = users.findById(studentId);

    if (!student) {
        std::cout << "Student not found.\n";
        return;
    }

    if (student->role != mlms::Role::Student) {
        std::cout << "The selected user is not a student.\n";
        return;
    }

    if (!student->active) {
        std::cout << "Cannot enroll an inactive student.\n";
        return;
    }

    const int courseId = readPositiveInt("Course ID: ");

    const auto course = courses.findById(courseId);

    if (!course) {
        std::cout << "Course not found.\n";
        return;
    }

    if (enrollments.isEnrolled(studentId, courseId)) {
        std::cout << "Student is already enrolled in this course.\n";
        return;
    }

    try {
        enrollments.enroll(studentId, courseId, "2026-10-26");
        std::cout << "Student enrolled successfully.\n";
    } catch (const std::exception& error) {
        std::cout << "Enrollment failed: "
                  << error.what() << '\n';
    }
}


void manageCourses(
    const mlms::AuthenticatedUser& currentUser,
    mlms::CourseRepository& courses,
    mlms::DepartmentRepository& departments,
    mlms::UserRepository& users,
    mlms::EnrollmentRepository& enrollments
) {
    mlms::AuthorizationGuard::requireAdministrator(currentUser.role);

    while (true) {
        std::cout
            << "\n========== COURSE MANAGEMENT ==========\n"
            << "1. Create course\n"
            << "2. List courses\n"
            << "3. Search course\n"
            << "4. Update course details\n"
            << "5. Assign or unassign lecturer\n"
	    << "6. Enroll student in course\n"
	    << "0. Return to dashboard\n"
            << "Select an option: ";

        std::string choice;
        if (!std::getline(std::cin, choice))
            return;

        if (choice == "1") {
            createCourse(courses, departments);
        } else if (choice == "2") {
            listCourses(courses);
        } else if (choice == "3") {
            searchCourse(courses);
        } else if (choice == "4") {
            updateCourse(courses);
        } else if (choice == "5") {
	    assignCourseLecturer(courses, users);
	} else if (choice == "6") {
	    enrollStudent(enrollments, users, courses);
	} else if (choice == "0") {
	    return;
	}
    }
}

void manageAssignments(
    const mlms::AuthenticatedUser& lecturer,
    mlms::CourseRepository& courses,
    mlms::AssignmentRepository& assignments
) {
    mlms::AuthorizationGuard::requireLecturer(lecturer.role);

    // Only courses assigned to this lecturer are available.
    auto getMyCourses = [&]() {
        std::vector<mlms::CourseRecord> result;

        for (const auto& course : courses.listAll()) {
            if (course.lecturerId &&
                *course.lecturerId == lecturer.userId) {
                result.push_back(course);
            }
        }

        return result;
    };

    auto findMyCourse = [&](int courseId)
        -> std::optional<mlms::CourseRecord> {
        const auto myCourses = getMyCourses();

        for (const auto& course : myCourses) {
            if (course.courseId == courseId)
                return course;
        }

        return std::nullopt;
    };

    while (true) {
        std::cout
            << "\n========== MANAGE ASSIGNMENTS ==========\n"
            << "1. Create assignment\n"
            << "2. List my assignments\n"
            << "3. Update assignment\n"
            << "4. Delete assignment\n"
            << "0. Return to dashboard\n"
            << "Select an option: ";

        std::string choice;
        if (!std::getline(std::cin, choice))
            return;

        if (choice == "0")
            return;

        if (choice == "1") {
            const auto myCourses = getMyCourses();

            if (myCourses.empty()) {
                std::cout << "No courses are assigned to you.\n";
                continue;
            }

            std::cout << "\nYour courses:\n";
            for (const auto& course : myCourses) {
                std::cout << course.courseId << ". "
                          << course.courseCode << " - "
                          << course.courseTitle << '\n';
            }

            const int courseId =
                readPositiveInt("Enter course ID: ");

            if (!findMyCourse(courseId)) {
                std::cout << "That course is not assigned to you.\n";
                continue;
            }

            const std::string title =
                readRequired("Assignment title: ");

            const std::string description =
                readRequired("Assignment description: ");

            const std::string deadline =
                readRequired(
                    "Deadline (YYYY-MM-DD HH:MM:SS): "
                );

            double maxMark = 0.0;
            while (true) {
                const std::string value =
                    readRequired("Maximum mark: ");

                try {
                    std::size_t used = 0;
                    maxMark = std::stod(value, &used);

                    if (used == value.size() &&
                        std::isfinite(maxMark) &&
                        maxMark > 0.0) {
                        break;
                    }
                } catch (const std::exception&) {
                    // Display validation message below.
                }

                std::cout
                    << "Enter a valid positive maximum mark.\n";
            }

            const int id = assignments.create(
                courseId,
                title,
                description,
                deadline,
                maxMark
            );

            std::cout
                << "Assignment created successfully. ID: "
                << id << '\n';

        } else if (choice == "2") {
            bool found = false;

            for (const auto& course : getMyCourses()) {
                for (const auto& assignment :
                     assignments.listByCourse(course.courseId)) {
                    found = true;

                    std::cout
                        << "\nAssignment ID: "
                        << assignment.assignmentId
                        << "\nCourse: " << course.courseCode
                        << "\nTitle: " << assignment.title
                        << "\nDescription: "
                        << assignment.description
                        << "\nDeadline: " << assignment.deadline
                        << "\nMaximum mark: "
                        << assignment.maxMark
                        << "\n------------------------------------\n";
                }
            }

            if (!found)
                std::cout << "No assignments found in your courses.\n";

        } else if (choice == "3") {
            const int id =
                readPositiveInt("Assignment ID to update: ");

            const auto assignment = assignments.findById(id);

            if (!assignment ||
                !findMyCourse(assignment->courseId)) {
                std::cout
                    << "Assignment not found or not assigned to you.\n";
                continue;
            }

            const std::string title =
                readRequired("New title: ");

            const std::string description =
                readRequired("New description: ");

            const std::string deadline =
                readRequired("New deadline (YYYY-MM-DD HH:MM:SS): ");

            double maxMark = 0.0;
            while (true) {
                const std::string value =
                    readRequired("New maximum mark: ");

                try {
                    std::size_t used = 0;
                    maxMark = std::stod(value, &used);

                    if (used == value.size() &&
                        std::isfinite(maxMark) &&
                        maxMark > 0.0) {
                        break;
                    }
                } catch (const std::exception&) {
                    // Display validation message below.
                }

                std::cout << "Enter a valid positive maximum mark.\n";
            }

            if (assignments.updateDetails(
                    id, title, description, deadline, maxMark)) {
                std::cout << "Assignment updated successfully.\n";
            } else {
                std::cout << "Assignment could not be updated.\n";
            }

        } else if (choice == "4") {
            const int id =
                readPositiveInt("Assignment ID to delete: ");

            const auto assignment = assignments.findById(id);

            if (!assignment ||
                !findMyCourse(assignment->courseId)) {
                std::cout
                    << "Assignment not found or not assigned to you.\n";
                continue;
            }

            std::cout
                << "Warning: deleting an assignment may be blocked "
                << "if submissions reference it.\n"
                << "Type YES to confirm deletion: ";

            std::string confirmation;
            if (!std::getline(std::cin, confirmation))
                return;

            if (confirmation != "YES") {
                std::cout << "Deletion cancelled.\n";
                continue;
            }

            try {
                if (assignments.deleteAssignment(id)) {
                    std::cout << "Assignment deleted successfully.\n";
                } else {
                    std::cout << "Assignment could not be deleted.\n";
                }
            } catch (const std::exception& error) {
                std::cout
                    << "Deletion failed: " << error.what() << '\n';
            }

        } else {
            std::cout << "Invalid option. Try again.\n";
        }
    }
}


void viewMyCourses(
    const mlms::AuthenticatedUser& currentUser,
    mlms::EnrollmentRepository& enrollments,
    mlms::CourseRepository& courses
) {
    mlms::AuthorizationGuard::requireStudent(currentUser.role);

    const auto studentEnrollments =
        enrollments.listByStudent(currentUser.userId);

    std::cout << "\n========== MY COURSES ==========\n";

    if (studentEnrollments.empty()) {
        std::cout << "You are not enrolled in any courses.\n";
        return;
    }

    for (const auto& enrollment : studentEnrollments) {
        const auto course = courses.findById(enrollment.courseId);

        if (!course) {
            std::cout << "Course ID " << enrollment.courseId
                      << " could not be found.\n";
            continue;
        }

        std::cout
            << "Course code: " << course->courseCode << '\n'
            << "Course title: " << course->courseTitle << '\n'
            << "Department ID: " << course->departmentId << '\n'
            << "Enrolled at: " << enrollment.enrolledAt << '\n'
            << "------------------------------------\n";
    }
}

std::string currentDateTime() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};
    localtime_r(&time, &localTime);

    std::ostringstream output;
    output << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return output.str();
}

std::time_t parseDateTime(const std::string& value) {
    std::tm parsed{};
    std::istringstream input(value);

    input >> std::get_time(&parsed, "%Y-%m-%d %H:%M:%S");

    if (input.fail()) {
        throw std::runtime_error(
            "Invalid assignment deadline format: " + value
        );
    }

    parsed.tm_isdst = -1;
    return std::mktime(&parsed);
}

int calculateDaysLate(
    const std::string& deadline,
    const std::string& submittedAt
) {
    const std::time_t due = parseDateTime(deadline);
    const std::time_t submitted = parseDateTime(submittedAt);

    if (submitted <= due)
        return 0;

    const double secondsLate = std::difftime(submitted, due);

    // Any submission after the deadline counts as at least one day late.
    return static_cast<int>(
        std::ceil(secondsLate / (24.0 * 60.0 * 60.0))
    );
}

void submitAssignment(
    const mlms::AuthenticatedUser& currentUser,
    mlms::EnrollmentRepository& enrollments,
    mlms::CourseRepository& courses,
    mlms::AssignmentRepository& assignments,
    mlms::SubmissionRepository& submissions
) {
    mlms::AuthorizationGuard::requireStudent(currentUser.role);

    const auto studentEnrollments =
        enrollments.listByStudent(currentUser.userId);

    if (studentEnrollments.empty()) {
        std::cout << "You must be enrolled in a course first.\n";
        return;
    }

    std::vector<mlms::AssignmentRecord> availableAssignments;

    for (const auto& enrollment : studentEnrollments) {
        const auto course = courses.findById(enrollment.courseId);

        if (!course)
            continue;

        const auto courseAssignments =
            assignments.listByCourse(enrollment.courseId);

        for (const auto& assignment : courseAssignments) {
            availableAssignments.push_back(assignment);

            std::cout
                << "\nAssignment ID: " << assignment.assignmentId
                << "\nCourse: " << course->courseCode
                << " - " << course->courseTitle
                << "\nTitle: " << assignment.title
                << "\nDescription: " << assignment.description
                << "\nDeadline: " << assignment.deadline
                << "\nMaximum mark: " << assignment.maxMark
                << "\n------------------------------------\n";
        }
    }

    if (availableAssignments.empty()) {
        std::cout << "No assignments are available in your courses.\n";
        return;
    }

    const int assignmentId =
        readPositiveInt("Enter the assignment ID to submit: ");

    const auto selected = std::find_if(
        availableAssignments.begin(),
        availableAssignments.end(),
        [assignmentId](const auto& assignment) {
            return assignment.assignmentId == assignmentId;
        }
    );

    if (selected == availableAssignments.end()) {
        std::cout
            << "Invalid assignment ID. You may only submit assignments "
            << "from courses in which you are enrolled.\n";
        return;
    }

    const std::string content =
        readRequired("Enter your submission content: ");

    const std::string submittedAt = currentDateTime();

    const int daysLate =
        calculateDaysLate(selected->deadline, submittedAt);

    const auto latest =
        submissions.findLatest(assignmentId, currentUser.userId);

    const int version = latest ? latest->version + 1 : 1;

    // The raw mark is not yet known at submission time.
    // The current penalty implementation calculates the amount from maxMark.
    const mlms::StandardLatePenalty penaltyPolicy(10.0, 30.0);

    const double penalty = penaltyPolicy.calculate(
        selected->maxMark,
        selected->maxMark,
        daysLate
    );

    const std::string state =
        daysLate > 0 ? "Late" : "Submitted";

    const int submissionId = submissions.create(
        assignmentId,
        currentUser.userId,
        version,
        submittedAt,
        content,
        state,
        daysLate,
        penalty
    );

    std::cout
        << "\nSubmission saved successfully.\n"
        << "Submission ID: " << submissionId
        << "\nVersion: " << version
        << "\nSubmitted at: " << submittedAt
        << "\nStatus: " << state
        << "\nDays late: " << daysLate
        << "\nRecorded penalty: " << penalty << " marks\n";
}

void reviewSubmissions(
    const mlms::AuthenticatedUser& currentUser,
    mlms::CourseRepository& courses,
    mlms::AssignmentRepository& assignments,
    mlms::SubmissionRepository& submissions
) {
    mlms::AuthorizationGuard::requireLecturer(currentUser.role);

    bool found = false;
    std::cout << "\n========== REVIEW SUBMISSIONS ==========\n";

    for (const auto& course : courses.listAll()) {
        if (!course.lecturerId ||
            *course.lecturerId != currentUser.userId) {
            continue;
        }

        for (const auto& assignment :
             assignments.listByCourse(course.courseId)) {
            for (const auto& submission :
                 submissions.listByAssignment(assignment.assignmentId)) {
                found = true;
                std::cout
                    << "\nSubmission ID: " << submission.submissionId
                    << "\nStudent ID: " << submission.studentId
                    << "\nCourse: " << course.courseCode
                    << "\nAssignment: " << assignment.title
                    << "\nVersion: " << submission.version
                    << "\nSubmitted at: " << submission.submittedAt
                    << "\nState: " << submission.state
                    << "\nDays late: " << submission.daysLate
                    << "\nContent:\n" << submission.content
                    << "\n------------------------------------\n";
            }
        }
    }

    if (!found) {
        std::cout << "No submissions found for your assigned courses.\n";
    }
}

void markSubmissions(
    const mlms::AuthenticatedUser& currentUser,
    mlms::CourseRepository& courses,
    mlms::AssignmentRepository& assignments,
    mlms::SubmissionRepository& submissions,
    mlms::MarkRepository& marks
) {
    mlms::AuthorizationGuard::requireLecturer(currentUser.role);

    struct PendingSubmission {
        mlms::SubmissionRecord submission;
        mlms::AssignmentRecord assignment;
    };

    std::vector<PendingSubmission> pending;

    for (const auto& course : courses.listAll()) {
        if (!course.lecturerId ||
            *course.lecturerId != currentUser.userId) {
            continue;
        }

        for (const auto& assignment :
             assignments.listByCourse(course.courseId)) {
            for (const auto& submission :
                 submissions.listByAssignment(assignment.assignmentId)) {
                if ((submission.state == "Submitted" ||
                     submission.state == "Late") &&
                    !marks.findBySubmission(submission.submissionId)) {
                    pending.push_back({submission, assignment});

                    std::cout
                        << "\nSubmission ID: " << submission.submissionId
                        << "\nStudent ID: " << submission.studentId
                        << "\nAssignment: " << assignment.title
                        << "\nMaximum mark: " << assignment.maxMark
                        << "\nState: " << submission.state
                        << "\nDays late: " << submission.daysLate
                        << "\n------------------------------------\n";
                }
            }
        }
    }

    if (pending.empty()) {
        std::cout << "No submissions are awaiting marking.\n";
        return;
    }

    const int submissionId =
        readPositiveInt("Enter submission ID to mark: ");

    const auto selected = std::find_if(
        pending.begin(), pending.end(),
        [submissionId](const auto& item) {
            return item.submission.submissionId == submissionId;
        }
    );

    if (selected == pending.end()) {
        std::cout << "Invalid submission ID for your assigned courses.\n";
        return;
    }

    double rawMark = 0.0;

    try {
        const std::string input = readRequired("Enter raw mark: ");
        std::size_t used = 0;
        rawMark = std::stod(input, &used);

        if (used != input.size() ||
            !std::isfinite(rawMark) ||
            rawMark < 0.0 ||
            rawMark > selected->assignment.maxMark) {
            std::cout << "Mark must be between 0 and "
                      << selected->assignment.maxMark << ".\n";
            return;
        }
    } catch (const std::exception&) {
        std::cout << "Invalid mark. Enter a valid number.\n";
        return;
    }

    const std::string feedback =
        readRequired("Enter feedback: ");

    const mlms::StandardLatePenalty penaltyPolicy(10.0, 30.0);
    const double penalty = penaltyPolicy.calculate(
        rawMark,
        selected->assignment.maxMark,
        selected->submission.daysLate
    );

    const double finalMark = std::max(0.0, rawMark - penalty);

    mlms::SubmissionState fromState;
    if (selected->submission.state == "Submitted") {
        fromState = mlms::SubmissionState::Submitted;
    } else if (selected->submission.state == "Late") {
        fromState = mlms::SubmissionState::Late;
    } else {
        std::cout << "This submission is not in a markable state.\n";
        return;
    }

    if (!mlms::canTransition(fromState, mlms::SubmissionState::Marked)) {
        std::cout << "Invalid submission state transition.\n";
        return;
    }

    try {
        const int markId = marks.create(
            selected->submission.submissionId,
            currentUser.userId,
            rawMark,
            finalMark,
            feedback,
            currentDateTime()
        );

        if (!submissions.updateState(
                selected->submission.submissionId, "Marked")) {
            std::cout
                << "Warning: mark " << markId
                << " was saved, but submission state could not be updated.\n";
            return;
        }

        std::cout
            << "\nMark saved successfully.\n"
            << "Mark ID: " << markId
            << "\nRaw mark: " << rawMark
            << "\nLate penalty: " << penalty
            << "\nFinal mark: " << finalMark << '\n';
    } catch (const std::exception& error) {
        std::cout << "Could not save mark: " << error.what() << '\n';
    }
}

void viewReleasedMarks(
    const mlms::AuthenticatedUser& user,
    mlms::AssignmentRepository& assignments,
    mlms::SubmissionRepository& submissions,
    mlms::CourseRepository& courses,
    mlms::MarkRepository& marks
) {
    std::cout << "\n========== RELEASED MARKS ==========\n";

    const auto studentSubmissions =
        submissions.listByStudent(user.userId);

    bool found = false;

    for (const auto& submission : studentSubmissions) {
        // Only display submissions that have been released.
        if (submission.state != "Released") {
            continue;
        }

        const auto mark =
            marks.findBySubmission(submission.submissionId);

        if (!mark || !mark->releasedAt.has_value()) {
            continue;
        }

        const auto assignment =
            assignments.findById(submission.assignmentId);

        if (!assignment) {
            continue;
        }

        const auto course =
            courses.findById(assignment->courseId);

        found = true;

        std::cout
            << "\nAssignment: " << assignment->title
            << "\nCourse: "
            << (course ? course->courseCode : "Unknown")
            << "\nRaw mark: " << mark->rawMark
            << "\nFinal mark: " << mark->finalMark
            << "\nFeedback: " << mark->feedback
            << "\nReleased at: " << *mark->releasedAt
            << "\n------------------------------------\n";
    }

    if (!found) {
        std::cout << "No released marks available.\n";
    }

    std::cout << "====================================\n";
}


void viewNotifications(
    const mlms::AuthenticatedUser& user,
    mlms::NotificationRepository& notifications
) {
    std::cout << "\n========== MY NOTIFICATIONS ==========\n";

    const auto items =
        notifications.listByRecipient(user.userId);

    if (items.empty()) {
        std::cout << "You have no notifications.\n";
        return;
    }

    for (const auto& item : items) {
        std::cout
            << "\nNotification ID: " << item.notificationId
            << "\nType: " << item.type
            << "\nMessage: " << item.message
            << "\nCreated at: " << item.createdAt
            << "\nStatus: "
            << (item.readAt ? "Read" : "Unread")
            << '\n'
            << "------------------------------------\n";
    }

    const auto unread =
        notifications.listUnreadByRecipient(user.userId);

    if (unread.empty()) {
        return;
    }

    std::cout << "Enter notification ID to mark as read (0 to return): ";

    std::string input;
    if (!std::getline(std::cin, input)) {
        return;
    }

    if (input == "0") {
        return;
    }

    int notificationId = 0;

    try {
        std::size_t position = 0;
        notificationId = std::stoi(input, &position);

        if (position != input.size() || notificationId <= 0) {
            std::cout << "Invalid notification ID.\n";
            return;
        }
    } catch (const std::exception&) {
        std::cout << "Invalid notification ID.\n";
        return;
    }

    const auto selected = notifications.findById(notificationId);

    if (!selected || selected->recipientId != user.userId) {
        std::cout << "Notification not found.\n";
        return;
    }

    if (selected->readAt) {
        std::cout << "This notification is already marked as read.\n";
        return;
    }

    if (notifications.markAsRead(
            notificationId, currentDateTime())) {
        std::cout << "Notification marked as read.\n";
    } else {
        std::cout << "Unable to mark notification as read.\n";
    }
}



int getHoDDepartmentId(
    const mlms::AuthenticatedUser& user,
    mlms::DepartmentRepository& departments
) {
    for (const auto& department : departments.listAll()) {
        if (department.hodId &&
            *department.hodId == user.userId) {
            return department.departmentId;
        }
    }

    return 0;
}


void showDepartmentOverview(
    const mlms::AuthenticatedUser& user,
    mlms::DepartmentRepository& departments,
    mlms::CourseRepository& courses,
    mlms::UserRepository& users
) {
    const int departmentId =
        getHoDDepartmentId(user, departments);

    std::cout << "\n========== DEPARTMENT OVERVIEW ==========\n";

    if (departmentId == 0) {
        std::cout << "You are not assigned to a department.\n";
        return;
    }

    const auto department = departments.findById(departmentId);

    if (!department) {
        std::cout << "Department record not found.\n";
        return;
    }

    std::cout
        << "Department ID: " << department->departmentId << '\n'
        << "Department name: " << department->departmentName << '\n';

    if (department->hodId) {
        const auto hod = users.findById(*department->hodId);

        std::cout << "Head of Department: "
                  << (hod ? hod->fullName : "User not found")
                  << '\n';
    } else {
        std::cout << "Head of Department: Not assigned\n";
    }

    std::cout << "\n---------- DEPARTMENT COURSES ----------\n";

    const auto departmentCourses =
        courses.listByDepartment(departmentId);

    if (departmentCourses.empty()) {
        std::cout << "No courses found in this department.\n";
        return;
    }

    for (const auto& course : departmentCourses) {
        std::cout
            << "\nCourse ID: " << course.courseId
            << "\nCourse code: " << course.courseCode
            << "\nCourse title: " << course.courseTitle
            << '\n';

        if (course.lecturerId) {
            const auto lecturer =
                users.findById(*course.lecturerId);

            std::cout << "Lecturer: "
                      << (lecturer
                              ? lecturer->fullName
                              : "User not found")
                      << '\n';
        } else {
            std::cout << "Lecturer: Not assigned\n";
        }

        std::cout << "----------------------------------------\n";
    }

    std::cout << "========================================\n";
}



void moderateMarks(
    const mlms::AuthenticatedUser& user,
    mlms::DepartmentRepository& departments,
    mlms::CourseRepository& courses,
    mlms::AssignmentRepository& assignments,
    mlms::SubmissionRepository& submissions,
    mlms::MarkRepository& marks,
    mlms::AuditRepository& audits
) {
    const int departmentId =
        getHoDDepartmentId(user, departments);

    if (departmentId == 0) {
        std::cout << "You are not assigned to a department.\n";
        return;
    }

    std::cout << "\n========== MARKS AWAITING MODERATION ==========\n";

    std::vector<int> eligibleSubmissionIds;
    bool found = false;

    for (const auto& assignment : assignments.listAll()) {
        const auto course = courses.findById(assignment.courseId);

        if (!course || course->departmentId != departmentId) {
            continue;
        }

        for (const auto& submission :
             submissions.listByAssignment(assignment.assignmentId)) {

            if (submission.state != "Marked") {
                continue;
            }

            const auto mark =
                marks.findBySubmission(submission.submissionId);

            if (!mark || mark->moderatedAt || mark->releasedAt) {
                continue;
            }

            found = true;
            eligibleSubmissionIds.push_back(submission.submissionId);

            std::cout
                << "\nSubmission ID: " << submission.submissionId
                << "\nAssignment: " << assignment.title
                << "\nRaw mark: " << mark->rawMark
                << "\nFinal mark: " << mark->finalMark
                << "\nFeedback: " << mark->feedback
                << "\n------------------------------------\n";
        }
    }

    if (!found) {
        std::cout << "No marks are awaiting moderation.\n";
        return;
    }

    const std::string input =
        readRequired("Enter submission ID to moderate (0 to cancel): ");

    int submissionId;

    try {
        submissionId = std::stoi(input);
    } catch (const std::exception&) {
        std::cout << "Invalid submission ID.\n";
        return;
    }

    if (submissionId == 0) {
        return;
    }

    if (std::find(
            eligibleSubmissionIds.begin(),
            eligibleSubmissionIds.end(),
            submissionId
        ) == eligibleSubmissionIds.end()) {
        std::cout << "That submission is not eligible for moderation.\n";
        return;
    }

    const auto submission = submissions.findById(submissionId);

    if (!submission || submission->state != "Marked") {
        std::cout << "Submission state has changed. Try again.\n";
        return;
    }

    const auto mark = marks.findBySubmission(submissionId);

    if (!mark || mark->moderatedAt || mark->releasedAt) {
        std::cout << "Mark is no longer eligible for moderation.\n";
        return;
    }

    const std::string timestamp = currentDateTime();

    if (!marks.recordModeration(
            mark->markId, user.userId, timestamp)) {
        std::cout << "Could not record moderation.\n";
        return;
    }

    if (!submissions.updateState(submissionId, "Moderated")) {
        std::cout
            << "Moderation was recorded, but submission state "
            << "could not be updated. Check the database.\n";
        return;
    }

    audits.create(
        user.userId,
        "Mark",
        mark->markId,
        "state",
        std::optional<std::string>("Marked"),
        "Moderated",
        "HoD moderated the mark",
        timestamp
    );

    std::cout << "Mark moderated successfully.\n";
}

void releaseModeratedMarks(
    const mlms::AuthenticatedUser& user,
    mlms::DepartmentRepository& departments,
    mlms::CourseRepository& courses,
    mlms::AssignmentRepository& assignments,
    mlms::SubmissionRepository& submissions,
    mlms::MarkRepository& marks,
    mlms::AuditRepository& audits,
    mlms::NotificationRepository& notifications
) {
    const int departmentId =
        getHoDDepartmentId(user, departments);

    if (departmentId == 0) {
        std::cout << "You are not assigned to a department.\n";
        return;
    }

    std::cout << "\n========== MODERATED MARKS ==========\n";

    std::vector<int> eligibleSubmissionIds;
    bool found = false;

    for (const auto& assignment : assignments.listAll()) {
        const auto course = courses.findById(assignment.courseId);

        if (!course || course->departmentId != departmentId) {
            continue;
        }

        for (const auto& submission :
             submissions.listByAssignment(assignment.assignmentId)) {

            if (submission.state != "Moderated") {
                continue;
            }

            const auto mark =
                marks.findBySubmission(submission.submissionId);

            if (!mark || !mark->moderatedAt || mark->releasedAt) {
                continue;
            }

            found = true;
            eligibleSubmissionIds.push_back(submission.submissionId);

            std::cout
                << "\nSubmission ID: " << submission.submissionId
                << "\nAssignment: " << assignment.title
                << "\nFinal mark: " << mark->finalMark
                << "\nFeedback: " << mark->feedback
                << "\nModerated at: " << *mark->moderatedAt
                << "\n------------------------------------\n";
        }
    }

    if (!found) {
        std::cout << "No moderated marks are awaiting release.\n";
        return;
    }

    const std::string input =
        readRequired("Enter submission ID to release (0 to cancel): ");

    int submissionId;

    try {
        submissionId = std::stoi(input);
    } catch (const std::exception&) {
        std::cout << "Invalid submission ID.\n";
        return;
    }

    if (submissionId == 0) {
        return;
    }

    if (std::find(
            eligibleSubmissionIds.begin(),
            eligibleSubmissionIds.end(),
            submissionId
        ) == eligibleSubmissionIds.end()) {
        std::cout << "That submission is not eligible for release.\n";
        return;
    }

    const auto submission = submissions.findById(submissionId);

    if (!submission || submission->state != "Moderated") {
        std::cout << "Submission state has changed. Try again.\n";
        return;
    }

    const auto mark = marks.findBySubmission(submissionId);

    if (!mark || !mark->moderatedAt || mark->releasedAt) {
        std::cout << "Mark is not eligible for release.\n";
        return;
    }

    const std::string timestamp = currentDateTime();

    if (!marks.releaseMark(mark->markId, timestamp)) {
        std::cout << "Could not release the mark.\n";
        return;
    }

    if (!submissions.updateState(submissionId, "Released")) {
        std::cout
            << "Mark was released in the mark record, but submission "
            << "state could not be updated. Check the database.\n";
        return;
    }

    audits.create(
        user.userId,
        "Mark",
        mark->markId,
        "released_at",
        std::nullopt,
        timestamp,
        "HoD released moderated mark",
        timestamp
    );

    notifications.create(
        submission->studentId,
        "MarkReleased",
        "Your mark for " + std::string("submission ") +
            std::to_string(submissionId) + " has been released.",
        std::optional<int>(mark->markId),
        timestamp
    );

    std::cout << "Mark released and student notified successfully.\n";
}


void showRoleMenu(
    const mlms::AuthenticatedUser& user,
    mlms::UserRepository& users,
    mlms::DepartmentRepository& departments,
    mlms::CourseRepository& courses,
    mlms::EnrollmentRepository& enrollments,
    mlms::AssignmentRepository& assignments,
    mlms::SubmissionRepository& submissions,
    mlms::MarkRepository& marks,
    mlms::AuditRepository& audits,
    mlms::NotificationRepository& notifications,
    const mlms::SodiumPasswordHasher& hasher
){
    while (true) {
        std::cout
            << "\n====================================\n"
            << "MLMS - " << mlms::toString(user.role) << " Dashboard\n"
            << "Welcome, " << user.fullName << '\n'
            << "Username: " << user.username << '\n'
            << "====================================\n";

        switch (user.role) {
            case mlms::Role::Student:
                std::cout << "1. My courses\n"
			  << "2. Submit an assignment\n"
			  << "3. View released marks\n"
			  << "4. View notifications\n";
				break;

            case mlms::Role::Lecturer:
                std::cout << "1. Manage assignments\n"
                          << "2. Review submissions\n"
                          << "3. Mark submissions\n";
                break;

            case mlms::Role::HoD:
	    std::cout << "1. Department overview\n"
		      << "2. Review marks for moderation\n"
		      << "3. Release moderated marks\n";
	    break;

            case mlms::Role::Administrator:
                std::cout << "1. Manage users\n"
                          << "2. Manage departments\n"
                          << "3. Manage courses\n";
                break;
        }

        std::cout << "0. Logout\nSelect an option: ";

        std::string choice;
        if (!std::getline(std::cin, choice))
            return;

        if (choice == "0") {
            std::cout << "You have been logged out.\n";
            return;
        }

        
        if (user.role == mlms::Role::Administrator && choice == "1") {
            manageUsers(user, users, hasher);
        } else if (
            user.role == mlms::Role::Administrator && choice == "2"
        ) {
            manageDepartments(user, departments, users);
        } else if (
            user.role == mlms::Role::Administrator && choice == "3"
        ) {
            manageCourses(user, courses, departments, users, enrollments);
        } else if (
            user.role == mlms::Role::Student && choice == "1"
        ) {
            viewMyCourses(user, enrollments, courses);
        } else if (
	    user.role == mlms::Role::Student && choice == "2"
	) {
	    submitAssignment(
		user,
		enrollments,
		courses,
		assignments,
		submissions
	    );
	} else if (
	    user.role == mlms::Role::Student && choice == "3"
	) {
	    viewReleasedMarks(
		user,
		assignments,
		submissions,
		courses,
		marks
	    );
	} else if (
	    user.role == mlms::Role::Student && choice == "4"
	) {
	    viewNotifications(user, notifications);
	} else if (
	    user.role == mlms::Role::Lecturer && choice == "1"
	) {
	    manageAssignments(user, courses, assignments);
	        } else if (
            user.role == mlms::Role::Lecturer && choice == "2"
        ) {
            reviewSubmissions(user, courses, assignments, submissions);
        } else if (
            user.role == mlms::Role::Lecturer && choice == "3"
        ) {
            markSubmissions(
                user, courses, assignments, submissions, marks
            );
	
        } else if (
            user.role == mlms::Role::HoD && choice == "1"
        ) {
            showDepartmentOverview(
                user,
                departments,
                courses,
                users
            );

	} else if (
            user.role == mlms::Role::HoD && choice == "2"
        ) {
            moderateMarks(
                user, departments, courses, assignments,
                submissions, marks, audits
            );
        } else if (
            user.role == mlms::Role::HoD && choice == "3"
        ) {
            releaseModeratedMarks(
                user, departments, courses, assignments,
                submissions, marks, audits, notifications
            );
        } else {
            std::cout
                << "Invalid option or feature not yet implemented.\n";
        }

    }
}

void runApplication() {
    mlms::SqliteConnection connection("mlms.db");

    mlms::SchemaInitializer::initialize(
        connection, "database/schema.sql");

    mlms::UserRepository users(connection);
    mlms::DepartmentRepository departments(connection);
    mlms::CourseRepository courses(connection);
    mlms::EnrollmentRepository enrollments(connection);
    mlms::AssignmentRepository assignments(connection);
    mlms::SubmissionRepository submissions(connection);
    mlms::MarkRepository marks(connection);
    mlms::AuditRepository audits(connection);
    mlms::NotificationRepository notifications(connection);
    mlms::SodiumPasswordHasher hasher;

    if (users.listAll().empty())
        createFirstAdministrator(users, hasher);

    mlms::AuthenticationService authentication(users, hasher);

    while (true) {
        std::cout
            << "\n====================================\n"
            << "MLMS - Moderated Learning Management System\n"
            << "====================================\n"
            << "1. Login\n"
            << "0. Exit\n"
            << "Select an option: ";

        std::string choice;
        if (!std::getline(std::cin, choice))
            break;

        if (choice == "0")
            break;

        if (choice != "1") {
            std::cout << "Invalid option. Try again.\n";
            continue;
        }

        const std::string username = readRequired("Username: ");
        const std::string password = readPassword("Password: ");

        const auto user = authentication.authenticate(username, password);

        if (!user) {
            std::cout << "Login failed. Check your credentials or account status.\n";
            continue;
        }

        std::cout << "\nLogin successful.\n";
        showRoleMenu(
	    *user,
	    users,
	    departments,
	    courses,
	    enrollments,
	    assignments,
	    submissions,
	    marks,
	    audits,
	    notifications,
	    hasher
	);
    }

    std::cout << "MLMS closed.\n";
}

} // namespace

int main() {
    try {
        runApplication();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "MLMS error: " << error.what() << '\n';
        return 1;
    }
}
