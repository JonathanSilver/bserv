DROP TABLE IF EXISTS db_test;
CREATE TABLE db_test (
	id serial PRIMARY KEY,
	name character varying(255) NOT NULL UNIQUE,
	active boolean NOT NULL,
	email character varying(255),
	code int
);