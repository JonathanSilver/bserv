DROP TABLE IF EXISTS ex_auth_user;
CREATE TABLE ex_auth_user (
    id serial PRIMARY KEY,
    username character varying(255) NOT NULL UNIQUE,
    password character varying(255) NOT NULL,
    is_active boolean NOT NULL,
    is_superuser boolean NOT NULL,
    first_name character varying(255),
    last_name character varying(255),
    email character varying(255)
);