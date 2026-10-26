#include "mlms/persistence/department_repository.hpp"
#include "mlms/domain/submission.hpp"
#include "mlms/domain/user_record.hpp"
#include "mlms/services/late_penalty.hpp"
#include "mlms/persistence/sqlite_connection.hpp"
#include "mlms/persistence/user_repository.hpp"
#include "mlms/core/errors.hpp"
#include "mlms/persistence/course_repository.hpp"
#include "mlms/persistence/user_repository.hpp"
#include "mlms/persistence/department_repository.hpp"
#include "mlms/persistence/enrollment_repository.hpp"
#include "mlms/persistence/assignment_repository.hpp"
#include "mlms/persistence/submission_repository.hpp"
#include "mlms/persistence/mark_repository.hpp"
#include "mlms/persistence/extension_repository.hpp"
#include "mlms/persistence/audit_repository.hpp"
#include "mlms/persistence/notification_repository.hpp"
#include "mlms/services/authorization_guard.hpp"
#include "mlms/services/authentication_service.hpp"
#include "mlms/persistence/statement.hpp"
#include "mlms/services/registration_service.hpp"
#include "mlms/security/sodium_password_hasher.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <limits>
#include <cmath>
#include <optional>

namespace {


class TestPasswordHasher : public mlms::IPasswordHasher {
public:
    std::string hashPassword(
        const std::string& password
    ) const override {
        return "test-hash:" + password;
    }

    bool verifyPassword(
        const std::string& password,
        const std::string& encodedHash
    ) const override {
        return encodedHash == "test-hash:" + password;
    }
};


void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Exception, typename Function>
void requireThrows(Function function, const char* message) {
    try {
        function();
    } catch (const Exception&) {
        return;
    }

    throw std::runtime_error(message);
}



void testAuthorizationGuard() {
    using mlms::AuthorizationGuard;
    using mlms::AuthorizationError;
    using mlms::Role;

    AuthorizationGuard::requireStudent(Role::Student);
    AuthorizationGuard::requireLecturer(Role::Lecturer);
    AuthorizationGuard::requireHoD(Role::HoD);
    AuthorizationGuard::requireAdministrator(
        Role::Administrator
    );

    requireThrows<AuthorizationError>(
        [] {
            AuthorizationGuard::requireStudent(
                Role::Lecturer
            );
        },
        "Student authorization must reject lecturers."
    );

    requireThrows<AuthorizationError>(
        [] {
            AuthorizationGuard::requireLecturer(
                Role::Student
            );
        },
        "Lecturer authorization must reject students."
    );

    requireThrows<AuthorizationError>(
        [] {
            AuthorizationGuard::requireHoD(
                Role::Administrator
            );
        },
        "HoD authorization must reject administrators."
    );

    requireThrows<AuthorizationError>(
        [] {
            AuthorizationGuard::requireAdministrator(
                Role::HoD
            );
        },
        "Administrator authorization must reject HoD users."
    );
}


void createTestSchema(mlms::SqliteConnection& connection) {
    connection.execute(R"SQL(
        CREATE TABLE user (
            user_id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL UNIQUE,
            password_hash TEXT NOT NULL,
            full_name TEXT NOT NULL,
            email TEXT NOT NULL,
            role TEXT NOT NULL CHECK (
                role IN ('Student', 'Lecturer', 'HoD', 'Administrator')
            ),
            dept_id INTEGER,
            student_no TEXT UNIQUE,
            staff_no TEXT UNIQUE,
            active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
            failed_login_attempts INTEGER NOT NULL DEFAULT 0
                CHECK (failed_login_attempts >= 0),
            locked_until TEXT
        );

        CREATE TABLE department (
            dept_id INTEGER PRIMARY KEY AUTOINCREMENT,
            dept_name TEXT NOT NULL UNIQUE,
            hod_id INTEGER REFERENCES user(user_id)
        );

        CREATE TABLE course (
            course_id INTEGER PRIMARY KEY AUTOINCREMENT,
            course_code TEXT NOT NULL UNIQUE,
            course_title TEXT NOT NULL,
            dept_id INTEGER NOT NULL REFERENCES department(dept_id),
            lecturer_id INTEGER REFERENCES user(user_id)
        );

        CREATE TABLE enrollment (
            student_id INTEGER NOT NULL REFERENCES user(user_id),
            course_id INTEGER NOT NULL REFERENCES course(course_id),
            enrolled_at TEXT NOT NULL,
            PRIMARY KEY (student_id, course_id)
        );

        CREATE TABLE assignment (
            assignment_id INTEGER PRIMARY KEY AUTOINCREMENT,
            course_id INTEGER NOT NULL REFERENCES course(course_id),
            title TEXT NOT NULL,
            description TEXT NOT NULL,
            deadline TEXT NOT NULL,
            max_mark REAL NOT NULL CHECK (max_mark > 0)
        );

        CREATE TABLE submission (
            submission_id INTEGER PRIMARY KEY AUTOINCREMENT,
            assignment_id INTEGER NOT NULL REFERENCES assignment(assignment_id),
            student_id INTEGER NOT NULL REFERENCES user(user_id),
            version INTEGER NOT NULL CHECK (version > 0),
            submitted_at TEXT NOT NULL,
            content TEXT NOT NULL,
            state TEXT NOT NULL CHECK (
                state IN ('Submitted', 'Late', 'Marked', 'Moderated', 'Released')
            ),
            days_late INTEGER NOT NULL DEFAULT 0 CHECK (days_late >= 0),
            penalty REAL NOT NULL DEFAULT 0 CHECK (penalty >= 0),
            UNIQUE (assignment_id, student_id, version)
        );

        CREATE TABLE mark (
            mark_id INTEGER PRIMARY KEY AUTOINCREMENT,
            submission_id INTEGER NOT NULL UNIQUE REFERENCES submission(submission_id),
            lecturer_id INTEGER NOT NULL REFERENCES user(user_id),
            raw_mark REAL NOT NULL CHECK (raw_mark >= 0),
            final_mark REAL NOT NULL CHECK (final_mark >= 0),
            feedback TEXT NOT NULL CHECK (length(feedback) > 0),
            marked_at TEXT NOT NULL,
            moderated_by INTEGER REFERENCES user(user_id),
            moderated_at TEXT,
            released_at TEXT
        );

        CREATE TABLE extension (
            extension_id INTEGER PRIMARY KEY AUTOINCREMENT,
            assignment_id INTEGER NOT NULL REFERENCES assignment(assignment_id),
            student_id INTEGER NOT NULL REFERENCES user(user_id),
            granted_by INTEGER NOT NULL REFERENCES user(user_id),
            new_deadline TEXT NOT NULL,
            reason TEXT NOT NULL CHECK (length(trim(reason)) > 0),
            granted_at TEXT NOT NULL
        );

        CREATE TABLE audit_entry (
            audit_id INTEGER PRIMARY KEY AUTOINCREMENT,
            changed_by INTEGER NOT NULL REFERENCES user(user_id),
            entity_type TEXT NOT NULL,
            entity_id INTEGER NOT NULL,
            field TEXT NOT NULL,
            old_value TEXT,
            new_value TEXT NOT NULL,
            reason TEXT NOT NULL,
            changed_at TEXT NOT NULL
        );

        CREATE TABLE notification (
            notification_id INTEGER PRIMARY KEY AUTOINCREMENT,
            recipient_id INTEGER NOT NULL REFERENCES user(user_id),
            type TEXT NOT NULL
                CHECK (type IN (
                    'MarkReleased',
                    'ModerationReturned',
                    'ExtensionGranted'
                )),
            message TEXT NOT NULL,
            related_entity_id INTEGER,
            created_at TEXT NOT NULL,
            read_at TEXT
        );
    )SQL");
}


mlms::UserRecord makeStudent() {
    mlms::UserRecord user;
    user.username = "student01";
    user.passwordHash = "test-hash-not-a-real-password";
    user.fullName = "Test Student";
    user.email = "student01@example.com";
    user.role = mlms::Role::Student;
    user.studentNumber = "STU001";
    return user;
}

void testSubmissionAndLatePenalty() {
    require(
        mlms::canTransition(
            mlms::SubmissionState::Submitted,
            mlms::SubmissionState::Marked
        ),
        "Submitted should transition to Marked"
    );

    require(
        !mlms::canTransition(
            mlms::SubmissionState::Released,
            mlms::SubmissionState::Marked
        ),
        "Released must be terminal"
    );

    mlms::StandardLatePenalty penalty(10.0, 30.0);

    require(
        std::abs(penalty.calculate(80.0, 100.0, 0) - 0.0) < 1e-9,
        "On-time submission should have no penalty"
    );

    require(
        std::abs(penalty.calculate(80.0, 100.0, 1) - 10.0) < 1e-9,
        "Day 1 should deduct 10% of the maximum mark"
    );

    require(
        std::abs(penalty.calculate(80.0, 100.0, 2) - 20.0) < 1e-9,
        "Day 2 should deduct 20% of the maximum mark"
    );

    require(
        std::abs(penalty.calculate(80.0, 100.0, 3) - 30.0) < 1e-9,
        "Day 3 should deduct 30% of the maximum mark"
    );

    require(
        std::abs(penalty.calculate(80.0, 100.0, 4) - 0.0) < 1e-9,
        "Day 4 should have zero penalty under FR13"
    );

    mlms::NoLatePenalty noPenalty;

    require(
        noPenalty.calculate(80.0, 100.0, 10) == 0.0,
        "NoLatePenalty must return zero"
    );
}

void testUserRepository() {
    mlms::SqliteConnection connection(":memory:");
    createTestSchema(connection);

    mlms::UserRepository repository(connection);

    // 1. Create a student.
    mlms::UserRecord student = makeStudent();
    const int studentId = repository.create(student);

    require(studentId > 0, "Created user ID must be positive");

    // 2. Find the student by ID.
    const auto byId = repository.findById(studentId);

    require(byId.has_value(), "Student should be found by ID");
    require(byId->userId == studentId, "User ID should match");
    require(byId->username == "student01", "Username should match");
    require(byId->fullName == "Test Student", "Full name should match");
    require(byId->email == "student01@example.com", "Email should match");
    require(byId->role == mlms::Role::Student, "Role should be Student");
    require(
        byId->studentNumber.has_value() &&
            *byId->studentNumber == "STU001",
        "Student number should match"
    );
    require(!byId->departmentId.has_value(), "Department should be null");
    require(byId->active, "New student should be active");

    // 3. Find the student by username.
    const auto byUsername = repository.findByUsername("student01");

    require(byUsername.has_value(), "Username lookup should succeed");
    require(
        byUsername->userId == studentId,
        "Username lookup should return the correct user"
    );

    // 4. Test username existence.
    require(
        repository.existsByUsername("student01"),
        "Existing username should be detected"
    );
    require(
        !repository.existsByUsername("missing-user"),
        "Missing username should not be detected"
    );

    // 5. Test listing users.
    auto users = repository.listAll();

    require(users.size() == 1, "There should be one user");

    // 6. Update the student's profile.
    require(
        repository.updateProfile(
            studentId,
            "Updated Student",
            "updated@example.com"
        ),
        "Existing user's profile should update"
    );

    const auto updated = repository.findById(studentId);

    require(updated.has_value(), "Updated user should still exist");
    require(
        updated->fullName == "Updated Student",
        "Updated full name should be saved"
    );
    require(
        updated->email == "updated@example.com",
        "Updated email should be saved"
    );

    require(
        !repository.updateProfile(
            999999,
            "Missing User",
            "missing@example.com"
        ),
        "Updating a missing user should return false"
    );

    // 7. Deactivate and reactivate the student.
    require(
        repository.setActive(studentId, false),
        "Existing user should be deactivated"
    );

    auto inactive = repository.findById(studentId);

    require(inactive.has_value() && !inactive->active,
            "Student should now be inactive");

    require(
        repository.setActive(studentId, true),
        "Existing user should be reactivated"
    );

    auto active = repository.findById(studentId);

    require(active.has_value() && active->active,
            "Student should now be active");

    require(
        !repository.setActive(999999, false),
        "Deactivating a missing user should return false"
    );

    // 8. Create a lecturer with a staff number.
    mlms::UserRecord lecturer;
    lecturer.username = "lecturer01";
    lecturer.passwordHash = "another-test-hash";
    lecturer.fullName = "Test Lecturer";
    lecturer.email = "lecturer01@example.com";
    lecturer.role = mlms::Role::Lecturer;
    lecturer.staffNumber = "STAFF001";

    const int lecturerId = repository.create(lecturer);

    const auto foundLecturer = repository.findById(lecturerId);

    require(
        foundLecturer.has_value() &&
            foundLecturer->role == mlms::Role::Lecturer,
        "Lecturer role should be stored and retrieved"
    );
    require(
        foundLecturer->staffNumber.has_value() &&
            *foundLecturer->staffNumber == "STAFF001",
        "Staff number should be stored and retrieved"
    );

    users = repository.listAll();

    require(users.size() == 2, "There should now be two users");
    require(
        users[0].userId < users[1].userId,
        "Users should be listed in ascending ID order"
    );

    // 9. Duplicate usernames must be rejected by the database.
    requireThrows<mlms::PersistenceError>(
        [&]() {
            repository.create(student);
        },
        "Duplicate username should raise PersistenceError"
    );

    // 10. Invalid input must be rejected.
    mlms::UserRecord invalidUser;
    invalidUser.username = "";
    invalidUser.passwordHash = "test-hash";
    invalidUser.fullName = "Invalid User";
    invalidUser.email = "invalid@example.com";

    requireThrows<mlms::ValidationError>(
        [&]() {
            repository.create(invalidUser);
        },
        "Empty username should raise ValidationError"
    );

    // 11. Missing lookups should return no result.
    require(
        !repository.findById(999999).has_value(),
        "Missing user ID should return no result"
    );
    require(
        !repository.findByUsername("missing-user").has_value(),
        "Missing username should return no result"
    );
}



void testAuthenticationService() {
    mlms::SqliteConnection connection(":memory:");
    createTestSchema(connection);

    mlms::UserRepository users(connection);
    TestPasswordHasher hasher;

    mlms::UserRecord student = makeStudent();
    student.passwordHash = hasher.hashPassword("CorrectPassword123!");

    const int studentId = users.create(student);
    mlms::AuthenticationService auth(users, hasher);

    // 1. Correct credentials must authenticate.
    const auto result = auth.authenticate(
        "student01",
        "CorrectPassword123!"
    );

    require(
        result.has_value(),
        "Correct credentials should authenticate."
    );

    require(
        result->userId == studentId,
        "Authenticated user ID should match."
    );

    require(
        result->role == mlms::Role::Student,
        "Authenticated role should be Student."
    );

    // 2. Incorrect password must fail.
    require(
        !auth.authenticate("student01", "WrongPassword123!").has_value(),
        "Incorrect password must be rejected."
    );

    // 3. Unknown username must fail.
    require(
        !auth.authenticate("unknown-user", "CorrectPassword123!").has_value(),
        "Unknown username must be rejected."
    );

    // 4. Empty credentials must fail.
    require(
        !auth.authenticate("", "CorrectPassword123!").has_value(),
        "Empty username must be rejected."
    );

    require(
        !auth.authenticate("student01", "").has_value(),
        "Empty password must be rejected."
    );

    // 5. Inactive accounts must fail.
    require(
        users.setActive(studentId, false),
        "Student should be deactivated for this test."
    );

    require(
        !auth.authenticate("student01", "CorrectPassword123!").has_value(),
        "Inactive accounts must be rejected."
    );

    // 6. Create a separate account for lockout tests.
    mlms::UserRecord lockoutStudent = makeStudent();
    lockoutStudent.username = "lockout-user";
    lockoutStudent.email = "lockout@example.com";
    lockoutStudent.studentNumber = "STU-LOCK";
    lockoutStudent.passwordHash =
        hasher.hashPassword("CorrectPassword123!");

    const int lockoutUserId = users.create(lockoutStudent);

    // 7. Four consecutive failures must not lock the account.
    for (int i = 0; i < 4; ++i) {
        require(
            !auth.authenticate(
                "lockout-user",
                "WrongPassword123!"
            ).has_value(),
            "Incorrect password must be rejected."
        );
    }

    require(
        !users.isLoginLocked(lockoutUserId),
        "Account must remain unlocked after four failures."
    );

    // 8. Successful login must reset the consecutive failure count.
    require(
        auth.authenticate(
            "lockout-user",
            "CorrectPassword123!"
        ).has_value(),
        "Correct credentials should succeed before lockout."
    );

    // Four more failures must still not lock the account.
    for (int i = 0; i < 4; ++i) {
        require(
            !auth.authenticate(
                "lockout-user",
                "WrongPassword123!"
            ).has_value(),
            "Incorrect password must be rejected after reset."
        );
    }

    require(
        !users.isLoginLocked(lockoutUserId),
        "Successful login must reset the failure counter."
    );

    // 9. The fifth consecutive failure must lock the account.
    require(
        !auth.authenticate(
            "lockout-user",
            "WrongPassword123!"
        ).has_value(),
        "The fifth incorrect password must be rejected."
    );

    require(
        users.isLoginLocked(lockoutUserId),
        "Account must be locked after five consecutive failures."
    );

    // 10. Even correct credentials must fail while locked.
    require(
        !auth.authenticate(
            "lockout-user",
            "CorrectPassword123!"
        ).has_value(),
        "Correct credentials must be rejected while locked."
    );

    // 11. Simulate an expired lock for testing.
    mlms::Statement expireLock(
        connection,
        "UPDATE user "
        "SET locked_until = datetime('now', '-1 minute') "
        "WHERE user_id = ?;"
    );

    expireLock.bind(1, lockoutUserId);
    expireLock.execute();

    require(
        !users.isLoginLocked(lockoutUserId),
        "An expired lock must no longer be active."
    );

    // 12. Correct credentials should work after the lock expires.
    require(
        auth.authenticate(
            "lockout-user",
            "CorrectPassword123!"
        ).has_value(),
        "Login should succeed after the lock expires."
    );

    require(
        !users.isLoginLocked(lockoutUserId),
        "Successful login after expiry must leave account unlocked."
    );
}

void testRegistrationService() {
    mlms::SqliteConnection connection(":memory:");
    createTestSchema(connection);

    TestPasswordHasher hasher;
    mlms::RegistrationService registration(connection, hasher);

    mlms::RegistrationRequest request{
        "newstudent",
        "StrongPassword123!",
        " New Student ",
        "newstudent@example.com",
        "",
        " STU100 ",
        ""
    };

    const long long id = registration.registerStudent(request);

    require(id > 0, "Registration must return a positive user ID");

    mlms::Statement query(
        connection,
        "SELECT username, password_hash, full_name, email, "
        "role, student_no FROM user WHERE user_id = ?;"
    );

    query.bind(1, static_cast<int>(id));

    require(query.step(), "Registered student must exist");

    require(
        query.columnText(0) == "newstudent",
        "Username must be stored correctly"
    );

    require(
        query.columnText(1) != request.password,
        "Password must not be stored as plaintext"
    );

    require(
        query.columnText(2) == "New Student",
        "Full name should be trimmed"
    );

    require(
        query.columnText(3) == "newstudent@example.com",
        "Email should be stored correctly"
    );

    require(
        query.columnText(4) == "Student",
        "Public registration must assign the Student role"
    );

    require(
        query.columnText(5) == "STU100",
        "Student number should be trimmed"
    );

    auto invalid = request;
    invalid.username = "ab";

    requireThrows<mlms::ValidationError>(
        [&]() {
            registration.registerStudent(invalid);
        },
        "Short usernames must be rejected"
    );

    invalid = request;
    invalid.password = "short";

    requireThrows<mlms::ValidationError>(
        [&]() {
            registration.registerStudent(invalid);
        },
        "Short passwords must be rejected"
    );

    invalid = request;
    invalid.fullName = "   ";

    requireThrows<mlms::ValidationError>(
        [&]() {
            registration.registerStudent(invalid);
        },
        "Blank full names must be rejected"
    );

    invalid = request;
    invalid.studentNumber = "   ";

    requireThrows<mlms::ValidationError>(
        [&]() {
            registration.registerStudent(invalid);
        },
        "Blank student numbers must be rejected"
    );

    // Duplicate username must be rejected.
    requireThrows<mlms::PersistenceError>(
        [&]() {
            registration.registerStudent(request);
        },
        "Duplicate usernames must be rejected"
    );

    // Duplicate student number must be rejected.
    auto duplicateStudentNumber = request;
    duplicateStudentNumber.username = "anotherstudent";
    duplicateStudentNumber.studentNumber = "STU100";

    requireThrows<mlms::PersistenceError>(
        [&]() {
            registration.registerStudent(duplicateStudentNumber);
        },
        "Duplicate student numbers must be rejected"
    );
}


void testDepartmentRepository() {
    mlms::SqliteConnection connection(":memory:");
    createTestSchema(connection);

    mlms::DepartmentRepository departments(connection);
    mlms::UserRepository users(connection);

    // 1. Create two departments.
    const int computingId = departments.create("Computer Systems");
    const int networksId = departments.create("Networking");

    require(computingId > 0, "Department ID must be positive");
    require(networksId > computingId, "Department IDs should increase");

    // 2. Find a department by ID.
    auto byId = departments.findById(computingId);

    require(byId.has_value(), "Department should be found by ID");
    require(
        byId->departmentName == "Computer Systems",
        "Department name should match"
    );
    require(
        !byId->hodId.has_value(),
        "New department should have no HoD"
    );

    // 3. Find a department by name.
    auto byName = departments.findByName("Networking");

    require(byName.has_value(), "Department should be found by name");
    require(
        byName->departmentId == networksId,
        "Found department ID should match"
    );

    require(
        !departments.findById(999999).has_value(),
        "Missing department ID should return no result"
    );
    require(
        !departments.findByName("Unknown Department").has_value(),
        "Missing department name should return no result"
    );

    // 4. List departments.
    auto all = departments.listAll();

    require(all.size() == 2, "There should be two departments");
    require(
        all[0].departmentId < all[1].departmentId,
        "Departments should be ordered by ID"
    );

    // 5. Update a department name.
    require(
        departments.updateName(computingId, "Computer Science"),
        "Existing department should be renamed"
    );

    byId = departments.findById(computingId);

    require(
        byId.has_value() &&
            byId->departmentName == "Computer Science",
        "Updated department name should be saved"
    );

    require(
        !departments.updateName(999999, "Missing Department"),
        "Renaming a missing department should return false"
    );

    // 6. Create a HoD user.
    mlms::UserRecord hod;
    hod.username = "hod01";
    hod.passwordHash = "test-hash-only";
    hod.fullName = "Test Head of Department";
    hod.email = "hod01@example.com";
    hod.role = mlms::Role::HoD;
    hod.staffNumber = "STAFF-HOD-01";

    const int hodId = users.create(hod);

    // 7. Assign the HoD.
    require(
        departments.assignHead(computingId, hodId),
        "Existing department should accept HoD assignment"
    );

    byId = departments.findById(computingId);

    require(
        byId.has_value() &&
            byId->hodId.has_value() &&
            *byId->hodId == hodId,
        "Assigned HoD ID should be saved"
    );

    require(
        !departments.assignHead(999999, hodId),
        "Assigning a HoD to a missing department should return false"
    );

    requireThrows<mlms::PersistenceError>(
        [&]() {
            departments.assignHead(computingId, 999999);
        },
        "A nonexistent HoD user should violate the foreign key"
    );

    // 8. Validate input.
    requireThrows<mlms::ValidationError>(
        [&]() {
            departments.create("");
        },
        "Empty department name should raise ValidationError"
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            departments.updateName(0, "Invalid Department");
        },
        "Invalid department ID should raise ValidationError"
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            departments.assignHead(computingId, 0);
        },
        "Invalid HoD ID should raise ValidationError"
    );

    // 9. Duplicate department names must be rejected.
    requireThrows<mlms::PersistenceError>(
        [&]() {
            departments.create("Networking");
        },
        "Duplicate department name should raise PersistenceError"
    );
}


void testCourseRepository() {
    mlms::SqliteConnection connection(":memory:");
    createTestSchema(connection);

    mlms::DepartmentRepository departmentRepo(connection);
    mlms::UserRepository userRepo(connection);
    mlms::CourseRepository courseRepo(connection);

    // Arrange: create a department.
    const int departmentId =
        departmentRepo.create("Computer Systems and Networks");

    // Arrange: create a lecturer.
    mlms::UserRecord lecturer;
    lecturer.username = "lecturer1";
    lecturer.passwordHash = "test-hash";
    lecturer.fullName = "Test Lecturer";
    lecturer.email = "lecturer@example.com";
    lecturer.role = mlms::Role::Lecturer;
    lecturer.departmentId = departmentId;
    lecturer.staffNumber = "STAFF001";

    const int lecturerId = userRepo.create(lecturer);

    // Test 1: Create a course.
    const int courseId = courseRepo.create(
        "CS401",
        "Computer Networks",
        departmentId,
        lecturerId
    );

    require(courseId > 0, "Course ID should be positive.");

    // Test 2: Find course by ID.
    auto byId = courseRepo.findById(courseId);

    require(byId.has_value(), "Course should be found by ID.");
    require(byId->courseCode == "CS401", "Course code should match.");
    require(
        byId->courseTitle == "Computer Networks",
        "Course title should match."
    );
    require(
        byId->departmentId == departmentId,
        "Course department should match."
    );
    require(
        byId->lecturerId.has_value() &&
        *byId->lecturerId == lecturerId,
        "Assigned lecturer should match."
    );

    // Test 3: Find course by code.
    auto byCode = courseRepo.findByCode("CS401");

    require(byCode.has_value(), "Course should be found by code.");
    require(
        byCode->courseId == courseId,
        "Course ID should match when searching by code."
    );

    // Test 4: Missing course lookups.
    require(
        !courseRepo.findById(9999).has_value(),
        "Missing course ID should return no result."
    );
    require(
        !courseRepo.findByCode("UNKNOWN").has_value(),
        "Missing course code should return no result."
    );

    // Test 5: List all courses.
    const auto allCourses = courseRepo.listAll();

    require(
        allCourses.size() == 1,
        "There should be one course."
    );

    // Test 6: List courses by department.
    const auto departmentCourses =
        courseRepo.listByDepartment(departmentId);

    require(
        departmentCourses.size() == 1,
        "Department should have one course."
    );

    require(
        departmentCourses.front().courseCode == "CS401",
        "Listed course should have the expected code."
    );

    // Test 7: Update course details.
    require(
        courseRepo.updateDetails(
            courseId,
            "CS402",
            "Advanced Computer Networks"
        ),
        "Existing course details should be updated."
    );

    auto updated = courseRepo.findById(courseId);

    require(updated.has_value(), "Updated course should exist.");
    require(
        updated->courseCode == "CS402",
        "Updated course code should match."
    );
    require(
        updated->courseTitle == "Advanced Computer Networks",
        "Updated course title should match."
    );

    require(
        !courseRepo.updateDetails(
            9999,
            "CS999",
            "Missing Course"
        ),
        "Updating a missing course should return false."
    );

    // Test 8: Unassign and reassign lecturer.
    require(
        courseRepo.assignLecturer(courseId, std::nullopt),
        "Lecturer should be unassignable."
    );

    auto unassigned = courseRepo.findById(courseId);

    require(
        unassigned.has_value() && !unassigned->lecturerId.has_value(),
        "Course should have no lecturer after unassignment."
    );

    require(
        courseRepo.assignLecturer(courseId, lecturerId),
        "Lecturer should be assignable."
    );

    // Test 9: Invalid input.
    requireThrows<mlms::ValidationError>(
        [&]() {
            courseRepo.create("", "Invalid Course", departmentId);
        },
        "Empty course code should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            courseRepo.create("CS500", "", departmentId);
        },
        "Empty course title should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            courseRepo.create("CS500", "Valid Title", 0);
        },
        "Invalid department ID should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            courseRepo.listByDepartment(0);
        },
        "Invalid department ID for listing should be rejected."
    );

    // Test 10: Duplicate course code.
    requireThrows<mlms::PersistenceError>(
        [&]() {
            courseRepo.create(
                "CS402",
                "Duplicate Course",
                departmentId
            );
        },
        "Duplicate course code should be rejected."
    );

    // Test 11: Foreign-key constraints.
    requireThrows<mlms::PersistenceError>(
        [&]() {
            courseRepo.create(
                "CS600",
                "Invalid Department",
                9999
            );
        },
        "Nonexistent department should be rejected."
    );

    requireThrows<mlms::PersistenceError>(
        [&]() {
            courseRepo.assignLecturer(courseId, 9999);
        },
        "Nonexistent lecturer user ID should be rejected."
    );
}


void testEnrollmentRepository() {
    mlms::SqliteConnection connection(":memory:");
    createTestSchema(connection);

    mlms::DepartmentRepository departmentRepo(connection);
    mlms::UserRepository userRepo(connection);
    mlms::CourseRepository courseRepo(connection);
    mlms::EnrollmentRepository enrollmentRepo(connection);

    // Create a department.
    const int departmentId =
        departmentRepo.create("Computer Systems");

    // Create two students.
    mlms::UserRecord student1;
    student1.username = "student1";
    student1.passwordHash = "test-hash";
    student1.fullName = "Student One";
    student1.email = "student1@example.com";
    student1.role = mlms::Role::Student;
    student1.departmentId = departmentId;
    student1.studentNumber = "ST001";

    const int studentId1 = userRepo.create(student1);

    mlms::UserRecord student2;
    student2.username = "student2";
    student2.passwordHash = "test-hash";
    student2.fullName = "Student Two";
    student2.email = "student2@example.com";
    student2.role = mlms::Role::Student;
    student2.departmentId = departmentId;
    student2.studentNumber = "ST002";

    const int studentId2 = userRepo.create(student2);

    // Create two courses.
    const int courseId1 = courseRepo.create(
        "CS401", "Computer Networks", departmentId
    );

    const int courseId2 = courseRepo.create(
        "CS402", "Operating Systems", departmentId
    );

    // Test 1: Enroll the first student.
    enrollmentRepo.enroll(studentId1, courseId1, "2026-10-24");

    require(
        enrollmentRepo.isEnrolled(studentId1, courseId1),
        "Student should be enrolled in the course."
    );

    // Test 2: Student is not enrolled in another course.
    require(
        !enrollmentRepo.isEnrolled(studentId1, courseId2),
        "Student should not be enrolled in the second course."
    );

    // Test 3: Enroll the second student in the first course.
    enrollmentRepo.enroll(studentId2, courseId1, "2026-10-24");

    // Test 4: List enrollments by student.
    auto studentEnrollments =
        enrollmentRepo.listByStudent(studentId1);

    require(
        studentEnrollments.size() == 1,
        "Student should have one enrollment."
    );

    require(
        studentEnrollments.front().courseId == courseId1,
        "Student enrollment should reference the correct course."
    );

    require(
        studentEnrollments.front().enrolledAt == "2026-10-24",
        "Enrollment date should match."
    );

    // Test 5: List enrollments by course.
    auto courseEnrollments =
        enrollmentRepo.listByCourse(courseId1);

    require(
        courseEnrollments.size() == 2,
        "Course should have two enrolled students."
    );

    // Test 6: Reject duplicate enrollment.
    requireThrows<mlms::PersistenceError>(
        [&]() {
            enrollmentRepo.enroll(
                studentId1, courseId1, "2026-10-25"
            );
        },
        "Duplicate enrollment should be rejected."
    );

    // Test 7: Reject invalid input.
    requireThrows<mlms::ValidationError>(
        [&]() {
            enrollmentRepo.enroll(0, courseId1, "2026-10-24");
        },
        "Invalid student ID should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            enrollmentRepo.enroll(studentId1, 0, "2026-10-24");
        },
        "Invalid course ID should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            enrollmentRepo.enroll(studentId1, courseId1, "");
        },
        "Empty enrollment date should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            enrollmentRepo.isEnrolled(0, courseId1);
        },
        "Invalid enrollment lookup IDs should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            enrollmentRepo.listByStudent(0);
        },
        "Invalid student ID for listing should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            enrollmentRepo.listByCourse(0);
        },
        "Invalid course ID for listing should be rejected."
    );

    // Test 8: Reject nonexistent foreign-key references.
    requireThrows<mlms::PersistenceError>(
        [&]() {
            enrollmentRepo.enroll(9999, courseId1, "2026-10-24");
        },
        "Nonexistent student should be rejected."
    );

    requireThrows<mlms::PersistenceError>(
        [&]() {
            enrollmentRepo.enroll(studentId1, 9999, "2026-10-24");
        },
        "Nonexistent course should be rejected."
    );

    // Test 9: Unenroll a student.
    require(
        enrollmentRepo.unenroll(studentId1, courseId1),
        "Existing enrollment should be deleted."
    );

    require(
        !enrollmentRepo.isEnrolled(studentId1, courseId1),
        "Student should no longer be enrolled."
    );

    require(
        !enrollmentRepo.unenroll(studentId1, courseId1),
        "Deleting a nonexistent enrollment should return false."
    );

    // Test 10: Verify remaining enrollment.
    require(
        enrollmentRepo.isEnrolled(studentId2, courseId1),
        "Unenrolling one student should not affect another."
    );
}


void testAssignmentRepository() {
    mlms::SqliteConnection connection(":memory:");
    createTestSchema(connection);

    mlms::DepartmentRepository departmentRepo(connection);
    mlms::CourseRepository courseRepo(connection);
    mlms::AssignmentRepository assignmentRepo(connection);

    // Arrange: create a department and course.
    const int departmentId =
        departmentRepo.create("Computer Systems");

    const int courseId = courseRepo.create(
        "CS401",
        "Computer Networks",
        departmentId
    );

    // Test 1: Create an assignment.
    const int assignmentId = assignmentRepo.create(
        courseId,
        "Network Design",
        "Design and document a small network.",
        "2026-11-10 23:59:00",
        100.0
    );

    require(
        assignmentId > 0,
        "Assignment ID should be positive."
    );

    // Test 2: Find assignment by ID.
    auto found = assignmentRepo.findById(assignmentId);

    require(found.has_value(), "Assignment should be found.");
    require(
        found->courseId == courseId,
        "Assignment course ID should match."
    );
    require(
        found->title == "Network Design",
        "Assignment title should match."
    );
    require(
        found->description ==
            "Design and document a small network.",
        "Assignment description should match."
    );
    require(
        found->deadline == "2026-11-10 23:59:00",
        "Assignment deadline should match."
    );
    require(
        found->maxMark == 100.0,
        "Assignment maximum mark should match."
    );

    // Test 3: Missing assignment.
    require(
        !assignmentRepo.findById(9999).has_value(),
        "Missing assignment should return no result."
    );

    // Test 4: List all assignments.
    const auto allAssignments = assignmentRepo.listAll();

    require(
        allAssignments.size() == 1,
        "There should be one assignment."
    );

    // Test 5: List assignments by course.
    const auto courseAssignments =
        assignmentRepo.listByCourse(courseId);

    require(
        courseAssignments.size() == 1,
        "Course should have one assignment."
    );

    require(
        courseAssignments.front().assignmentId == assignmentId,
        "Listed assignment ID should match."
    );

    // Test 6: Update assignment details.
    require(
        assignmentRepo.updateDetails(
            assignmentId,
            "Updated Network Design",
            "Submit the revised network design.",
            "2026-11-15 23:59:00",
            80.0
        ),
        "Existing assignment should be updated."
    );

    auto updated = assignmentRepo.findById(assignmentId);

    require(updated.has_value(), "Updated assignment should exist.");
    require(
        updated->title == "Updated Network Design",
        "Updated title should match."
    );
    require(
        updated->maxMark == 80.0,
        "Updated maximum mark should match."
    );

    require(
        !assignmentRepo.updateDetails(
            9999,
            "Missing",
            "Missing assignment",
            "2026-11-20",
            50.0
        ),
        "Updating a missing assignment should return false."
    );

    // Test 7: Reject invalid input.
    requireThrows<mlms::ValidationError>(
        [&]() {
            assignmentRepo.create(
                0, "Title", "Description", "2026-11-10", 100.0
            );
        },
        "Invalid course ID should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            assignmentRepo.create(
                courseId, "", "Description", "2026-11-10", 100.0
            );
        },
        "Empty title should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            assignmentRepo.create(
                courseId, "Title", "", "2026-11-10", 100.0
            );
        },
        "Empty description should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            assignmentRepo.create(
                courseId, "Title", "Description", "", 100.0
            );
        },
        "Empty deadline should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            assignmentRepo.create(
                courseId, "Title", "Description", "2026-11-10", 0.0
            );
        },
        "Zero maximum mark should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            assignmentRepo.create(
                courseId, "Title", "Description",
                "2026-11-10", -1.0
            );
        },
        "Negative maximum mark should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            assignmentRepo.create(
                courseId, "Title", "Description",
                "2026-11-10",
                std::numeric_limits<double>::infinity()
            );
        },
        "Infinite maximum mark should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            assignmentRepo.findById(0);
        },
        "Invalid assignment ID should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            assignmentRepo.listByCourse(0);
        },
        "Invalid course ID for listing should be rejected."
    );

    // Test 8: Reject nonexistent course.
    requireThrows<mlms::PersistenceError>(
        [&]() {
            assignmentRepo.create(
                9999,
                "Invalid Course Assignment",
                "Description",
                "2026-11-10",
                100.0
            );
        },
        "Nonexistent course should be rejected."
    );

    // Test 9: Delete assignment.
    require(
        assignmentRepo.deleteAssignment(assignmentId),
        "Existing assignment should be deleted."
    );

    require(
        !assignmentRepo.findById(assignmentId).has_value(),
        "Deleted assignment should not be found."
    );

    require(
        !assignmentRepo.deleteAssignment(assignmentId),
        "Deleting a missing assignment should return false."
    );
}


void testSubmissionRepository() {
    mlms::SqliteConnection connection(":memory:");
    createTestSchema(connection);

    mlms::DepartmentRepository departmentRepo(connection);
    mlms::UserRepository userRepo(connection);
    mlms::CourseRepository courseRepo(connection);
    mlms::AssignmentRepository assignmentRepo(connection);
    mlms::SubmissionRepository submissionRepo(connection);

    // Arrange: create a department, course and assignment.
    const int departmentId =
        departmentRepo.create("Computer Systems");

    const int courseId = courseRepo.create(
        "CS401", "Computer Networks", departmentId
    );

    const int assignmentId = assignmentRepo.create(
        courseId,
        "Network Design",
        "Submit a network design.",
        "2026-11-10 23:59:00",
        100.0
    );

    // Arrange: create a student.
    mlms::UserRecord student;
    student.username = "student1";
    student.passwordHash = "test-hash";
    student.fullName = "Student One";
    student.email = "student1@example.com";
    student.role = mlms::Role::Student;
    student.departmentId = departmentId;
    student.studentNumber = "ST001";

    const int studentId = userRepo.create(student);

    // Test 1: Create the first submission.
    const int submissionId = submissionRepo.create(
        assignmentId,
        studentId,
        1,
        "2026-11-09 14:00:00",
        "Initial network design",
        "Submitted",
        0,
        0.0
    );

    require(submissionId > 0, "Submission ID should be positive.");

    // Test 2: Retrieve submission.
    auto found = submissionRepo.findById(submissionId);

    require(found.has_value(), "Submission should be found.");
    require(found->assignmentId == assignmentId,
            "Assignment ID should match.");
    require(found->studentId == studentId,
            "Student ID should match.");
    require(found->version == 1, "Version should be one.");
    require(found->content == "Initial network design",
            "Submission content should match.");
    require(found->state == "Submitted",
            "Submission state should match.");
    require(found->daysLate == 0, "Days late should be zero.");
    require(found->penalty == 0.0, "Penalty should be zero.");

    // Test 3: Find the latest submission.
    auto latest = submissionRepo.findLatest(assignmentId, studentId);

    require(latest.has_value(), "Latest submission should exist.");
    require(latest->version == 1, "Latest version should be one.");

    // Test 4: Create a later version.
    const int secondSubmissionId = submissionRepo.create(
        assignmentId,
        studentId,
        2,
        "2026-11-09 16:00:00",
        "Revised network design",
        "Submitted",
        0,
        0.0
    );

    latest = submissionRepo.findLatest(assignmentId, studentId);

    require(latest.has_value(), "Latest submission should exist.");
    require(latest->submissionId == secondSubmissionId,
            "Latest submission should be version two.");
    require(latest->version == 2, "Latest version should be two.");

    // Test 5: List by assignment and student.
    const auto byAssignment =
        submissionRepo.listByAssignment(assignmentId);

    require(byAssignment.size() == 2,
            "Assignment should have two submissions.");

    const auto byStudent =
        submissionRepo.listByStudent(studentId);

    require(byStudent.size() == 2,
            "Student should have two submissions.");

    // Test 6: Record a late submission.
    const int lateSubmissionId = submissionRepo.create(
        assignmentId,
        studentId,
        3,
        "2026-11-12 10:00:00",
        "Late network design",
        "Late",
        2,
        20.0
    );

    auto late = submissionRepo.findById(lateSubmissionId);

    require(late.has_value(), "Late submission should exist.");
    require(late->state == "Late", "State should be Late.");
    require(late->daysLate == 2, "Late days should be two.");
    require(late->penalty == 20.0, "Stored penalty should be 20.");

    // Test 7: Update submission state.
    require(
        submissionRepo.updateState(submissionId, "Marked"),
        "Existing submission state should be updated."
    );

    found = submissionRepo.findById(submissionId);

    require(found.has_value(), "Updated submission should exist.");
    require(found->state == "Marked",
            "Submission state should be Marked.");

    require(
        !submissionRepo.updateState(9999, "Marked"),
        "Updating a missing submission should return false."
    );

    // Test 8: Reject invalid input.
    requireThrows<mlms::ValidationError>(
        [&]() {
            submissionRepo.create(
                0, studentId, 1, "2026-11-09",
                "Content", "Submitted", 0, 0.0
            );
        },
        "Invalid assignment ID should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            submissionRepo.create(
                assignmentId, studentId, 0, "2026-11-09",
                "Content", "Submitted", 0, 0.0
            );
        },
        "Invalid version should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            submissionRepo.create(
                assignmentId, studentId, 4, "2026-11-09",
                "", "Submitted", 0, 0.0
            );
        },
        "Empty content should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            submissionRepo.create(
                assignmentId, studentId, 4, "2026-11-09",
                "Content", "Unknown", 0, 0.0
            );
        },
        "Invalid state should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            submissionRepo.create(
                assignmentId, studentId, 4, "2026-11-09",
                "Content", "Submitted", -1, 0.0
            );
        },
        "Negative late days should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            submissionRepo.create(
                assignmentId, studentId, 4, "2026-11-09",
                "Content", "Submitted", 0, -1.0
            );
        },
        "Negative penalty should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            submissionRepo.create(
                assignmentId, studentId, 4, "2026-11-09",
                "Content", "Submitted", 0,
                std::numeric_limits<double>::infinity()
            );
        },
        "Infinite penalty should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            submissionRepo.create(
                assignmentId, studentId, 4, "2026-11-09",
                "Content", "Submitted", 1, 10.0
            );
        },
        "Late days require the Late state."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            submissionRepo.create(
                assignmentId, studentId, 4, "2026-11-09",
                "Content", "Late", 0, 0.0
            );
        },
        "Late state requires positive late days."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            submissionRepo.findById(0);
        },
        "Invalid submission ID should be rejected."
    );

    // Test 9: Reject nonexistent foreign-key references.
    requireThrows<mlms::PersistenceError>(
        [&]() {
            submissionRepo.create(
                9999, studentId, 1, "2026-11-09",
                "Content", "Submitted", 0, 0.0
            );
        },
        "Nonexistent assignment should be rejected."
    );

    requireThrows<mlms::PersistenceError>(
        [&]() {
            submissionRepo.create(
                assignmentId, 9999, 1, "2026-11-09",
                "Content", "Submitted", 0, 0.0
            );
        },
        "Nonexistent student should be rejected."
    );

    // Test 10: Reject duplicate version.
    requireThrows<mlms::PersistenceError>(
        [&]() {
            submissionRepo.create(
                assignmentId, studentId, 1, "2026-11-09",
                "Duplicate version", "Submitted", 0, 0.0
            );
        },
        "Duplicate assignment/student/version should be rejected."
    );

    // Test 11: Reject invalid listing IDs.
    requireThrows<mlms::ValidationError>(
        [&]() {
            submissionRepo.listByAssignment(0);
        },
        "Invalid assignment ID for listing should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            submissionRepo.listByStudent(0);
        },
        "Invalid student ID for listing should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            submissionRepo.findLatest(0, studentId);
        },
        "Invalid assignment ID for latest lookup should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            submissionRepo.updateState(submissionId, "InvalidState");
        },
        "Invalid state update should be rejected."
    );
}

void testMarkRepository() {
    mlms::SqliteConnection connection(":memory:");
    createTestSchema(connection);

    // Create the department and its users.
    connection.execute(R"(
        INSERT INTO department (dept_name)
        VALUES ('Computer Systems');

        INSERT INTO user
            (username, password_hash, full_name, email, role)
        VALUES
            ('lecturer1', 'hash1', 'Test Lecturer',
            'lecturer@example.com', 'Lecturer');

        INSERT INTO user
            (username, password_hash, full_name, email, role)
        VALUES
            ('hod1', 'hash2', 'Test HoD',
            'hod@example.com', 'HoD');

        INSERT INTO user
            (username, password_hash, full_name, email, role)
        VALUES
            ('student1', 'hash3', 'Test Student',
            'student@example.com', 'Student');

        INSERT INTO course (course_code, course_title, dept_id, lecturer_id)
        VALUES ('CS101', 'Computer Systems', 1, 1);

        INSERT INTO assignment
            (course_id, title, description, deadline, max_mark)
        VALUES (1, 'Test Assignment', 'Assignment description',
                '2026-10-20 23:59:59', 100);

        INSERT INTO submission
            (assignment_id, student_id, version, submitted_at,
            content, state, days_late, penalty)
        VALUES
            (1, 3, 1, '2026-10-20 12:00:00',
            'Test submission', 'Submitted', 0, 0);
    )");

    mlms::MarkRepository repository(connection);

    // 1. Create a mark.
    const int markId = repository.create(
        1, 1, 85.0, 85.0,
        "Good work.", "2026-10-21 10:00:00"
    );
    require(markId > 0, "Mark creation should return a valid ID.");

    // 2. Retrieve the mark by ID.
    auto mark = repository.findById(markId);
    require(mark.has_value(), "Created mark should be found.");
    require(mark->rawMark == 85.0, "Raw mark should be 85.");
    require(mark->finalMark == 85.0, "Final mark should be 85.");
    require(mark->feedback == "Good work.", "Feedback should match.");

    // 3. Retrieve the mark by submission ID.
    auto bySubmission = repository.findBySubmission(1);
    require(bySubmission.has_value(),
            "Mark should be found using submission ID.");

    // 4. Update the mark before release.
    require(repository.updateMark(
                markId, 90.0, 90.0, "Excellent work."),
            "An unreleased mark should be updateable.");

    mark = repository.findById(markId);
    require(mark->rawMark == 90.0, "Updated raw mark should be 90.");
    require(mark->feedback == "Excellent work.",
            "Updated feedback should match.");

    // 5. Record moderation.
    require(repository.recordModeration(
                markId, 2, "2026-10-22 09:00:00"),
            "Moderation should be recorded.");

    mark = repository.findById(markId);
    require(mark->moderatedBy.has_value(),
            "Moderator ID should be recorded.");
    require(*mark->moderatedBy == 2, "Moderator ID should be 2.");
    require(mark->moderatedAt.has_value(),
            "Moderation date should be recorded.");

    // 6. Release the moderated mark.
    require(repository.releaseMark(
                markId, "2026-10-23 12:00:00"),
            "A moderated mark should be releasable.");

    mark = repository.findById(markId);
    require(mark->releasedAt.has_value(),
            "Release date should be recorded.");

    // 7. A released mark must not be editable or released again.
    require(!repository.updateMark(
                markId, 95.0, 95.0, "Changed after release."),
            "A released mark must not be updateable.");

    require(!repository.releaseMark(
                markId, "2026-10-24 12:00:00"),
            "A released mark must not be released again.");

    // 8. Reject invalid input.
    requireThrows<mlms::ValidationError>(
        [&]() {
            repository.create(
                1, 1, -1.0, 50.0, "Invalid mark",
                "2026-10-24 10:00:00"
            );
        },
        "Negative marks should be rejected.");

    requireThrows<mlms::ValidationError>(
        [&]() {
            repository.findById(0);
        },
        "A non-positive mark ID should be rejected.");
}

void testExtensionRepository() {
    mlms::SqliteConnection connection(":memory:");
    createTestSchema(connection);

    // Create the department and users.
    connection.execute(R"SQL(
        INSERT INTO department (dept_name)
        VALUES ('Computer Systems');

        INSERT INTO user
            (username, password_hash, full_name, email, role)
        VALUES
            ('lecturer1', 'hash1', 'Test Lecturer',
             'lecturer@example.com', 'Lecturer');

        INSERT INTO user
            (username, password_hash, full_name, email, role)
        VALUES
            ('hod1', 'hash2', 'Test HoD',
             'hod@example.com', 'HoD');

        INSERT INTO user
            (username, password_hash, full_name, email, role)
        VALUES
            ('student1', 'hash3', 'Test Student',
             'student@example.com', 'Student');

        INSERT INTO user
            (username, password_hash, full_name, email, role)
        VALUES
            ('student2', 'hash4', 'Second Student',
             'student2@example.com', 'Student');

        INSERT INTO course
            (course_code, course_title, dept_id, lecturer_id)
        VALUES
            ('CS101', 'Computer Systems', 1, 1);

        INSERT INTO assignment
            (course_id, title, description, deadline, max_mark)
        VALUES
            (1, 'Test Assignment', 'Assignment description',
             '2026-10-20 23:59:59', 100);
    )SQL");

    mlms::ExtensionRepository repository(connection);

    // 1. Create an extension.
    const int extensionId = repository.create(
        1,
        3,
        2,
        "2026-10-25 23:59:59",
        "Medical circumstances",
        "2026-10-21 09:00:00"
    );

    require(
        extensionId > 0,
        "Extension creation should return a valid ID."
    );

    // 2. Find the extension by ID.
    auto extension = repository.findById(extensionId);

    require(extension.has_value(), "Created extension should be found.");
    require(extension->assignmentId == 1, "Assignment ID should match.");
    require(extension->studentId == 3, "Student ID should match.");
    require(extension->grantedBy == 2, "Grantor ID should match.");
    require(
        extension->newDeadline == "2026-10-25 23:59:59",
        "New deadline should match."
    );
    require(
        extension->reason == "Medical circumstances",
        "Extension reason should match."
    );

    // 3. A nonexistent extension should not be found.
    require(
        !repository.findById(999).has_value(),
        "A nonexistent extension should not be found."
    );

    // 4. List extensions for a student.
    const auto studentExtensions = repository.listByStudent(3);

    require(
        studentExtensions.size() == 1,
        "Student should have one extension."
    );

    // 5. List extensions for an assignment.
    const auto assignmentExtensions = repository.listByAssignment(1);

    require(
        assignmentExtensions.size() == 1,
        "Assignment should have one extension."
    );

    // 6. Create a second extension for another student.
    const int secondExtensionId = repository.create(
        1,
        4,
        2,
        "2026-10-27 23:59:59",
        "Approved academic circumstances",
        "2026-10-22 10:00:00"
    );

    require(
        secondExtensionId > extensionId,
        "Second extension should receive a new ID."
    );

    require(
        repository.listByAssignment(1).size() == 2,
        "Assignment should now have two extensions."
    );

    require(
        repository.listByStudent(4).size() == 1,
        "Second student should have one extension."
    );

    // 7. Invalid IDs must be rejected.
    requireThrows<mlms::ValidationError>(
        [&]() {
            repository.findById(0);
        },
        "Zero extension ID should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            repository.listByStudent(0);
        },
        "Zero student ID should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            repository.listByAssignment(-1);
        },
        "Negative assignment ID should be rejected."
    );

    // 8. Blank reasons must be rejected.
    requireThrows<mlms::ValidationError>(
        [&]() {
            repository.create(
                1,
                3,
                2,
                "2026-10-28 23:59:59",
                "   ",
                "2026-10-22 11:00:00"
            );
        },
        "Blank extension reason should be rejected."
    );

    // 9. Invalid foreign-key references must be rejected.
    requireThrows<mlms::PersistenceError>(
        [&]() {
            repository.create(
                999,
                3,
                2,
                "2026-10-28 23:59:59",
                "Valid reason",
                "2026-10-22 11:00:00"
            );
        },
        "Nonexistent assignment should be rejected."
    );
}

void testAuditRepository() {
    mlms::SqliteConnection connection(":memory:");
    createTestSchema(connection);

    connection.execute(R"SQL(
        INSERT INTO user
            (username, password_hash, full_name, email, role)
        VALUES
            ('admin1', 'hash1', 'Test Administrator',
             'admin@example.com', 'Administrator');

        INSERT INTO user
            (username, password_hash, full_name, email, role)
        VALUES
            ('lecturer1', 'hash2', 'Test Lecturer',
             'lecturer@example.com', 'Lecturer');
    )SQL");

    mlms::AuditRepository repository(connection);

    // 1. Create an audit record with an old value.
    const int firstId = repository.create(
        1,
        "Course",
        10,
        "course_title",
        std::optional<std::string>{"Networking"},
        "Advanced Networking",
        "Course title updated",
        "2026-10-24 10:00:00"
    );

    require(firstId > 0, "Audit creation should return a valid ID.");

    // 2. Find the record by ID.
    auto record = repository.findById(firstId);

    require(record.has_value(), "Created audit record should be found.");
    require(record->changedBy == 1, "Changed-by user ID should match.");
    require(record->entityType == "Course", "Entity type should match.");
    require(record->entityId == 10, "Entity ID should match.");
    require(record->field == "course_title", "Field should match.");
    require(
        record->oldValue.has_value() &&
            *record->oldValue == "Networking",
        "Old value should match."
    );
    require(
        record->newValue == "Advanced Networking",
        "New value should match."
    );
    require(
        record->reason == "Course title updated",
        "Reason should match."
    );

    // 3. Create an audit record with a NULL old value.
    const int secondId = repository.create(
        2,
        "Assignment",
        20,
        "deadline",
        std::nullopt,
        "2026-11-01 23:59:59",
        "Initial deadline recorded",
        "2026-10-24 11:00:00"
    );

    record = repository.findById(secondId);

    require(record.has_value(), "Second audit record should be found.");
    require(
        !record->oldValue.has_value(),
        "Missing old value should remain NULL."
    );

    // 4. List records for an entity.
    const auto courseHistory =
        repository.listByEntity("Course", 10);

    require(
        courseHistory.size() == 1,
        "Course should have one audit record."
    );

    // 5. List records by user.
    const auto adminHistory = repository.listByUser(1);

    require(
        adminHistory.size() == 1,
        "Administrator should have one audit record."
    );

    const auto lecturerHistory = repository.listByUser(2);

    require(
        lecturerHistory.size() == 1,
        "Lecturer should have one audit record."
    );

    // 6. A nonexistent audit record should return no result.
    require(
        !repository.findById(999).has_value(),
        "Nonexistent audit record should not be found."
    );

    // 7. Invalid IDs and required values should be rejected.
    requireThrows<mlms::ValidationError>(
        [&]() {
            repository.findById(0);
        },
        "Zero audit ID should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            repository.listByUser(0);
        },
        "Zero user ID should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            repository.listByEntity("", 10);
        },
        "Blank entity type should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            repository.create(
                1, "Course", 10, "course_title",
                std::nullopt, "New title", "   ",
                "2026-10-24 12:00:00"
            );
        },
        "Blank reason should be rejected."
    );

    // 8. A nonexistent user must be rejected by the foreign key.
    requireThrows<mlms::PersistenceError>(
        [&]() {
            repository.create(
                999, "Course", 10, "course_title",
                std::nullopt, "New title", "Test reason",
                "2026-10-24 12:00:00"
            );
        },
        "Nonexistent changed-by user should be rejected."
    );
}


void testNotificationRepository() {
    mlms::SqliteConnection connection(":memory:");
    createTestSchema(connection);

    connection.execute(R"SQL(
        INSERT INTO user
            (username, password_hash, full_name, email, role)
        VALUES
            ('student1', 'hash1', 'Test Student',
             'student@example.com', 'Student');

        INSERT INTO user
            (username, password_hash, full_name, email, role)
        VALUES
            ('lecturer1', 'hash2', 'Test Lecturer',
             'lecturer@example.com', 'Lecturer');
    )SQL");

    mlms::NotificationRepository repository(connection);

    // 1. Create a notification with a related entity.
    const int firstId = repository.create(
        1,
        "MarkReleased",
        "Your assignment mark has been released.",
        10,
        "2026-10-24 10:00:00"
    );

    require(firstId > 0, "Notification ID should be valid.");

    // 2. Retrieve the notification by ID.
    auto notification = repository.findById(firstId);

    require(notification.has_value(),
        "Created notification should be found.");
    require(notification->recipientId == 1,
        "Recipient ID should match.");
    require(notification->type == "MarkReleased",
        "Notification type should match.");
    require(notification->relatedEntityId.has_value() &&
            *notification->relatedEntityId == 10,
        "Related entity ID should match.");
    require(!notification->readAt.has_value(),
        "New notification should be unread.");

    // 3. Create a notification without a related entity.
    const int secondId = repository.create(
        1,
        "ExtensionGranted",
        "Your extension has been granted.",
        std::nullopt,
        "2026-10-24 11:00:00"
    );

    notification = repository.findById(secondId);

    require(notification.has_value(),
        "Second notification should be found.");
    require(!notification->relatedEntityId.has_value(),
        "Missing related entity should remain NULL.");

    // 4. List all notifications for a recipient.
    const auto studentNotifications =
        repository.listByRecipient(1);

    require(studentNotifications.size() == 2,
        "Student should have two notifications.");

    // 5. List unread notifications.
    auto unreadNotifications =
        repository.listUnreadByRecipient(1);

    require(unreadNotifications.size() == 2,
        "Both notifications should initially be unread.");

    // 6. Mark the first notification as read.
    require(
        repository.markAsRead(firstId, "2026-10-24 12:00:00"),
        "Unread notification should be marked as read."
    );

    notification = repository.findById(firstId);

    require(notification.has_value() &&
            notification->readAt.has_value() &&
            *notification->readAt == "2026-10-24 12:00:00",
        "Read timestamp should be stored.");

    // 7. A notification already read should not be updated again.
    require(
        !repository.markAsRead(firstId, "2026-10-24 13:00:00"),
        "Already-read notification should not be updated."
    );

    unreadNotifications = repository.listUnreadByRecipient(1);

    require(unreadNotifications.size() == 1,
        "Only one notification should remain unread.");

    // 8. Another recipient should have no notifications.
    require(
        repository.listByRecipient(2).empty(),
        "Lecturer should have no notifications."
    );

    // 9. Missing notification IDs should return no result.
    require(
        !repository.findById(999).has_value(),
        "Nonexistent notification should not be found."
    );

    require(
        !repository.markAsRead(999, "2026-10-24 14:00:00"),
        "Nonexistent notification should not be marked as read."
    );

    // 10. Invalid inputs should be rejected.
    requireThrows<mlms::ValidationError>(
        [&]() {
            repository.findById(0);
        },
        "Zero notification ID should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            repository.listByRecipient(0);
        },
        "Zero recipient ID should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            repository.listUnreadByRecipient(0);
        },
        "Zero unread-list recipient ID should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            repository.create(
                1, "InvalidType", "Test message",
                std::nullopt, "2026-10-24 15:00:00"
            );
        },
        "Invalid notification type should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            repository.create(
                1, "MarkReleased", "   ",
                std::nullopt, "2026-10-24 15:00:00"
            );
        },
        "Blank notification message should be rejected."
    );

    requireThrows<mlms::ValidationError>(
        [&]() {
            repository.markAsRead(firstId, "   ");
        },
        "Blank read timestamp should be rejected."
    );

    // 11. Foreign-key violations should be rejected.
    requireThrows<mlms::PersistenceError>(
        [&]() {
            repository.create(
                999, "MarkReleased", "Test message",
                std::nullopt, "2026-10-24 16:00:00"
            );
        },
        "Nonexistent recipient should be rejected."
    );
}

void testSodiumPasswordHasher() {
    mlms::SodiumPasswordHasher hasher;

    const std::string password = "StrongPassword123!";

    const std::string hash1 = hasher.hashPassword(password);
    const std::string hash2 = hasher.hashPassword(password);

    require(!hash1.empty(), "Password hash must not be empty");

    require(
        hash1 != password,
        "Password hash must not contain plaintext"
    );

    require(
        hash1 != hash2,
        "Repeated hashes should use different salts"
    );

    require(
        hasher.verifyPassword(password, hash1),
        "Correct password must verify"
    );

    require(
        !hasher.verifyPassword("WrongPassword123!", hash1),
        "Incorrect password must fail verification"
    );

    require(
        !hasher.verifyPassword("", hash1),
        "Empty password must fail verification"
    );

    require(
        hash1.rfind("$argon2id$", 0) == 0,
        "Hash must identify the Argon2id algorithm"
    );
}


} // namespace

int main() {
    try {
        testSodiumPasswordHasher();
        testAuthenticationService();
        testRegistrationService();
        testAuthorizationGuard();
        testUserRepository();
        testDepartmentRepository();
        testCourseRepository();
        testEnrollmentRepository();
        testAssignmentRepository();
        testSubmissionRepository();
        testMarkRepository();
        testExtensionRepository();
        testAuditRepository();
        testNotificationRepository();

        std::cout << "All MLMS tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return 1;
    }
}
