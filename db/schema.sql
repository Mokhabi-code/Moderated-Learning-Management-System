CREATE TABLE department (
    dept_id INTEGER PRIMARY KEY AUTOINCREMENT, 
    dept_name TEXT NOT NULL UNIQUE, 
    hod_id INTEGER REFERENCES 
    user(user_id));

CREATE TABLE user (
    user_id INTEGER PRIMARY KEY AUTOINCREMENT, 
    username TEXT NOT NULL UNIQUE, 
    password_hash TEXT NOT NULL, 
    full_name TEXT NOT NULL, 
    email TEXT NOT NULL, role TEXT NOT NULL CHECK (role IN ('Student','Lecturer','HoD','Administrator')), 
    dept_id INTEGER REFERENCES department(dept_id), 
    student_no TEXT UNIQUE, 
    staff_no TEXT UNIQUE, 
    active INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)));

CREATE TABLE course (
    course_id INTEGER PRIMARY KEY AUTOINCREMENT, 
    course_code TEXT NOT NULL UNIQUE, 
    course_title TEXT NOT NULL, 
    dept_id INTEGER NOT NULL REFERENCES department(dept_id), 
    lecturer_id INTEGER REFERENCES user(user_id));

CREATE TABLE enrollment (
    student_id INTEGER NOT NULL REFERENCES user(user_id), 
    course_id INTEGER NOT NULL REFERENCES course(course_id), 
    enrolled_at TEXT NOT NULL, 
    PRIMARY KEY (student_id, course_id));

CREATE TABLE assignment (
    assignment_id INTEGER PRIMARY KEY AUTOINCREMENT, 
    course_id INTEGER NOT NULL REFERENCES course(course_id), 
    title TEXT NOT NULL, 
    description TEXT NOT NULL, 
    deadline TEXT NOT NULL, 
    max_mark REAL NOT NULL CHECK (max_mark > 0));

CREATE TABLE submission (
    submission_id INTEGER PRIMARY KEY AUTOINCREMENT, 
    assignment_id INTEGER NOT NULL REFERENCES assignment(assignment_id), 
    student_id INTEGER NOT NULL REFERENCES user(user_id), 
    version INTEGER NOT NULL CHECK (version > 0), 
    submitted_at TEXT NOT NULL, 
    content TEXT NOT NULL, 
    state TEXT NOT NULL CHECK (state IN ('Submitted','Late','Marked','Moderated','Released')), 
    days_late INTEGER NOT NULL DEFAULT 0 CHECK (days_late >= 0), 
    penalty REAL NOT NULL DEFAULT 0 CHECK (penalty >= 0), 
    UNIQUE (assignment_id, student_id, version));

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
    released_at TEXT);

CREATE TABLE extension (
    extension_id INTEGER PRIMARY KEY AUTOINCREMENT, 
    assignment_id INTEGER NOT NULL REFERENCES assignment(assignment_id), 
    student_id INTEGER NOT NULL REFERENCES user(user_id), 
    granted_by INTEGER NOT NULL REFERENCES user(user_id), 
    new_deadline TEXT NOT NULL, 
    reason TEXT NOT NULL, 
    granted_at TEXT NOT NULL);

CREATE TABLE audit_entry (
    audit_id INTEGER PRIMARY KEY AUTOINCREMENT, 
    changed_by INTEGER NOT NULL REFERENCES user(user_id), 
    entity_type TEXT NOT NULL, 
    entity_id INTEGER NOT NULL, 
    field TEXT NOT NULL, 
    old_value TEXT, 
    new_value TEXT NOT NULL, 
    reason TEXT NOT NULL, 
    changed_at TEXT NOT NULL);

CREATE TABLE notification (
    notification_id INTEGER PRIMARY KEY AUTOINCREMENT, 
    recipient_id INTEGER NOT NULL REFERENCES user(user_id), 
    type TEXT NOT NULL CHECK (type IN ('MarkReleased','ModerationReturned','ExtensionGranted')), 
    message TEXT NOT NULL, 
    related_entity_id INTEGER, 
    created_at TEXT NOT NULL, 
    read_at TEXT);

CREATE INDEX idx_notification_recipient ON notification(recipient_id);
CREATE INDEX idx_user_username ON user(username);
CREATE INDEX idx_submission_assignment_student ON submission(assignment_id, student_id);
CREATE INDEX idx_mark_submission ON mark(submission_id);
CREATE INDEX idx_audit_entity ON audit_entry(entity_type, entity_id);
CREATE INDEX idx_extension_student_assignment ON extension(student_id, assignment_id);

