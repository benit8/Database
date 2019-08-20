C++ Database
============
An OOP, single-header, encapsulation of sqlite3.

You must obviously install the sqlite3 development files to compile with it:
```
$ sudo apt-get install sqlite3 libsqlite3-dev
```

Compile with:
```
$ g++ SomeSourceFile.cpp -lsqlite3
```

## Contents
1. [Database](#database)
2. [Statement](#statement)
3. [Row](#row)
4. [Value](#value)
5. [Blob](#blob)


## Database
#### Constructor
Opens/creates a database file
```C++
SQL::Database db("server.db");
```

#### Database::exec()
Runs a single SQL statement on the database
```C++
db.exec("CREATE TABLE `users`(`id` INT NOT NULL AUTO_INCREMENT, `name` VARCHAR(255) NOT NULL, `age` INT NOT NULL);");
```

#### Database::query()
Applys a callback to a set of results
```C++
db.query("SELECT `id`, `name` FROM `users`", [](SQL::Row &row) {
	std::cout << "User #" << row["id"].text() << ": " << row["name"].text() << std::endl;
	return true;
});
```
Return false in your callback to stop the looping.

#### Database::lastInsertId()
Gets the last inserted row id
```C++
std::int64_t id = db.lastInsertId();
```

#### Database::prepare()
Instanciates a prepared statement
```C++
SQL::Statement stmt = db.prepare("INSERT INTO `users`(`name`, `age`) VALUES ('John', 23)");
```


## Statement
#### Statement::operator bool()
Checks if a statement has been constructed successfuly
```C++
SQL::Statement stmt = db.prepare("Hello, food ?");
if (!stmt) {
	std::cerr << "Food machine broke" << std::endl;
	// exit, throw, return false...
}
```
When compiled and run, the above outputs:
```
$> ./a.out
Database prepare failed: near "Hello": syntax error
Food machine broke
```

#### Statement::bind()
Binds values to a statement
```C++
SQL::Statement stmt = db.prepare("INSERT INTO `users`(`name`, `age`) VALUES (?, ?)");
stmt.bind(1, "John");
stmt.bind(2, 23);
/// This doesn't execute the statement
```

#### Statement::execute()
Executes a statement (both snippets have the same effect)
```C++
SQL::Statement stmt = db.prepare("INSERT INTO `users`(`name`, `age`) VALUES (?, ?)");
stmt.bind(1, "John");
stmt.bind(2, 23);
bool success = stmt.execute();
```
```C++
SQL::Statement stmt = db.prepare("INSERT INTO `users`(`name`, `age`) VALUES (?, ?)");
bool success = stmt.execute("John", 23);
```

#### Statement::fetch()
Fetch rows one at a time from a statement
```C++
SQL::Statement stmt = db.prepare("SELECT * FROM `users` WHERE `age` > ?");
if (!stmt.execute(18))
	// exit, throw, return false...
Row user;
while (stmt.fetch(user))
	std::cout << user["name"].text() << " is major" << std::endl;
```

#### Statement::fetchAll()
Fetches **all** results from a statement
```C++
SQL::Statement stmt = db.prepare("SELECT * FROM `users` WHERE `age` > ?");
if (!stmt.execute(18))
	// exit, throw, return false...
std::vector<Row> users = stmt.fetchAll();
for (auto user : users)
	std::cout << user["name"].text() << " is major" << std::endl;
```

#### Statement::reset()
Resets a statement for re-use
```C++
Row user;
SQL::Statement stmt = db.prepare("SELECT * FROM `users`");

while (stmt.fetch(user))
	// We iterate on `users`

stmt.reset(); /// Let's go for another round

while (stmt.fetch(user))
	// We re-iterate on `users`
```

#### Statement::colCount()
Gets the number of columns of the current fetched row
```C++
std::size_t count = stmt.colCount();
```

#### Statement::colName()
Gets the nth column name of the current fetched row
```C++
std::string name = stmt.colName(1);
```

#### Statement::colValue()
Gets the nth column value of the current fetched row
```C++
SQL::Value val = stmt.colValue(1);
```

#### Statement::colSize()
Gets the nth column size of the current fetched row
```C++
std::size_t size = stmt.colSize(1);
```
>Size is in bytes, and is only applicable to BLOBs and TEXTs

#### Statement::queryString()
Gets the original query string back
```C++
std::string query = stmt.queryString();
```


## Row
An `SQL::Row` is simply a typedef of an `std::unordered_map<std::string, SQL::Value>`.
You can access the column's values in a standard way:
```C++
SQL::Row row;
stmt.fetch(row);

SQL::Value name = row["name"];
std::cout << name.text() << std::endl;
```


## Value
The `SQL::Value` is an encapsulation of `sqlite3_value`, which is an opaque union-like structure.
It can store integers, decimals, strings and blobs.

```C++
SQL::Value val = someRow["someColumn"];

int     i32 = val.integer();
int64_t i64 = val.bigInteger();
double real = val.real();
string text = val.text();
Blob   blob = val.blob();
```


## Blob
The `SQL::Blob` is a simple structure that can be described as follows:
```C++
struct Blob
{
	const void *data;
	std::size_t size;
};
```
Its goal is simply to contain an untyped set of bytes and to prevent too much arguments in the methods' signatures (e.g. no third parameter for [`Statement::bind()`](#statementbind) when given a `const void *`).