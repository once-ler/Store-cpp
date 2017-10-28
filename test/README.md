* _Commands_ which request that something should happen, ie some state change
* _Events_ which indicate that something has happened
* _Aggregates_ that handles Commands and generates Events based on the current state
  * A Command is always sent to a single Aggregate.
* _Event store_ which stores all events that has happened
* _Application services_ that receives Commands and routes it to the appropriate aggregate
  * Load events for the event store
  * Instantiate a new aggregate
  * Applies all events on the aggregate
  * Sends the command to the aggregate
  * Store the new events

References:
https://blog.jayway.com/2013/03/08/aggregates-event-sourcing-distilled/
https://dev.to/barryosull/event-sourcing-what-it-is-and-why-its-awesome

Event sourcing builds a data model based on event series.

Most enterprise systems built with a concept that of events are generated after data is written to the relational data model.

The reason being they have been using these events primarily for system integration ( messaging ).

The status quo in enterprise is to write to a relational model, then broadcast events based on those changes.

In effect, the events are projections of the relational data, which in my mind is putting the cart before the horse.

This is a flavour of CQRS, and is considered a stepping stone in migrating to an Event Sourced system, rather than the end result.

MSH-9 Message/Event Type
MSH-10 Main Identifier created by sender
MSH-11 Processing ID (D Debugging, T Training, P Production)
MSH-15 Accept Acknowledgement Type (Original: ACID, Enhanced: Eventual Consistency)
MSH-16 Application Acknowledgement Type (AL Always, NE Never, ER Error only, SU Successful only)

# Command is immutable
```
type BaseCommand {
  UUID commandId
}

type SomeCommand<T> : BaseCommand {

}

```

# Create Command Service
```
Response create(Context ctx) {
  sendCommand(SomeCommand<Context>{});
  return Response{};
}

```

# Events are just immutable data
```
type SomeEvent<T> : Event {
  UUID commandId
}
```

# Aggregates
```
// Implement handlers for commands that generate events.  State is not modified.
List<Event> handle(SomeCommand<T> c) {
  return List{
    SomeEvent<A>{},
    SomeEvent<B>{}
  };
}

// Implement handlers for events that modify state
// we only store state in the aggregate that we actually use for command procesing
void handle(SomeEvent<T> e) {
  this.createdAt = now();
  this.data = e.data;
}

```

# EventStore
```
public interface EventStore {
  EventStream loadEventStream(UUID aggregateId);
  void store(UUID aggregateId, long version, List<Event> events);
}

```

#EventStream
```
public interface EventStream : Iterable<event> {
  long version();
}
```

# Application Service
```
public void handle(Command command) throws Exception {
  EventStream<long> eventStream = eventStore.loadEventStream(command.aggregateId());
  Object target = newAggregateInstance(command);
  for (Event event : eventStream) {
    // The event processing is always trivial and should never fail because the event has already happened.
    handle(target, event);
  }
  List<event> events = handle(target, command);
  if (events != null && events.size() > 0) {
    eventStore.store(command.aggregateId(), eventStream.version(), events);
  } else {
    // Command generated no events
  }
}
```

Reference: https://raw.githubusercontent.com/JasperFx/marten/master/src/Marten/Schema/SQL/mt_stream.sql
```sql
DROP TABLE IF EXISTS master.mt_streams CASCADE;
CREATE TABLE master.mt_streams (
	id					uuid CONSTRAINT pk_mt_streams PRIMARY KEY,
	type				varchar(100) NULL,
	version				integer NOT NULL,
	timestamp			timestamptz default (now()) NOT NULL,
	snapshot			jsonb,
	snapshot_version	integer	
);

DROP SEQUENCE IF EXISTS master.mt_events_sequence;
CREATE SEQUENCE master.mt_events_sequence;

DROP TABLE IF EXISTS master.mt_events;
CREATE TABLE master.mt_events (
	seq_id		bigint CONSTRAINT pk_mt_events PRIMARY KEY,
	id		uuid NOT NULL,
	stream_id	uuid REFERENCES master.mt_streams ON DELETE CASCADE,
	version		integer NOT NULL,
	data		jsonb NOT NULL,
	type 		varchar(100) NOT NULL,
	timestamp	timestamptz default (now()) NOT NULL,
	CONSTRAINT pk_mt_events_stream_and_version UNIQUE(stream_id, version),
	CONSTRAINT pk_mt_events_id_unique UNIQUE(id)
);

ALTER SEQUENCE master.mt_events_sequence OWNED BY master.mt_events.seq_id;

select uuid_generate_v4();

select master.mt_append_event('06606420-0bc4-4661-8f4a-9a04ef7caea1', 'FOO_ED', '{"25bc6c1a-1beb-4dd9-b0ed-15094c579f01"}', '{"BAR_ED"}', array['{"sender":"pablo","body":"they are on to us"}']::jsonb[]);

select master.mt_append_event('06606420-0bc4-4661-8f4a-9a04ef7caea1', 'FOO_ED', '{"7a7e431a-d6d6-46a8-8c34-21e52368de70", "95f3d695-e826-4305-b76d-fb5046b650e9"}', '{"BAR_ED", "BAR_ED"}', array['{"sender":"pablo","body":"they are on to us"}', '{"sender":"samantha","body":"they are on to us 2"}']::jsonb[]);

select master.mt_append_event('06606420-0bc4-4661-8f4a-9a04ef7caea1', 'FOO_ED', array[uuid_generate_v4(), uuid_generate_v4()]::uuid[], '{"BAR_ED", "BAR_ED"}', array['{"sender":"pablo","body":"they are on to us"}', '{"sender":"samantha","body":"they are on to us 2"}']::jsonb[]);

CREATE OR REPLACE FUNCTION master.mt_append_event(stream uuid, stream_type varchar, event_ids uuid[], event_types varchar[], bodies jsonb[]) RETURNS int[] AS $$
DECLARE
	event_version int;
	event_type varchar;
	event_id uuid;
	body jsonb;
	index int;
	seq int;
	return_value int[];
BEGIN
	select version into event_version from master.mt_streams where id = stream;
	if event_version IS NULL then
		event_version = 0;
		insert into master.mt_streams (id, type, version, timestamp) values (stream, stream_type, 0, now());
	end if;


	index := 1;
	return_value := ARRAY[event_version + array_length(event_ids, 1)];

	foreach event_id in ARRAY event_ids
	loop
	    seq := nextval('master.mt_events_sequence');
		return_value := array_append(return_value, seq);

	    event_version := event_version + 1;
		event_type = event_types[index];
		body = bodies[index];

		insert into master.mt_events 
			(seq_id, id, stream_id, version, data, type) 
		values 
			(seq, event_id, stream, event_version, body, event_type);

		
		index := index + 1;
	end loop;

	update master.mt_streams set version = event_version, timestamp = now() where id = stream;

	return return_value;
END
$$ LANGUAGE plpgsql;




DROP TABLE IF EXISTS master.mt_event_progression CASCADE;
CREATE TABLE master.mt_event_progression (
	name				varchar CONSTRAINT pk_mt_event_progression PRIMARY KEY,
	last_seq_id			bigint NULL
);



CREATE OR REPLACE FUNCTION master.mt_mark_event_progression(name varchar, last_encountered bigint) RETURNS VOID LANGUAGE plpgsql AS $function$
BEGIN
INSERT INTO master.mt_event_progression (name, last_seq_id) VALUES (name, last_encountered)
  ON CONFLICT ON CONSTRAINT pk_mt_event_progression
  DO UPDATE SET last_seq_id = last_encountered;

END;
$function$;
```

# Projections
* Projection - any strategy for generating "read side" views from the raw event streams
* Transformation - a type of projection that generates or updates a single read side view for a single event
* Aggregate - a type of projection that "aggregates" data from multiple events to create a single readside view document
* Inline Projections - projection's that are executed as part of any event capture transaction
* Async Projections - projection's that run in some kind of background process using an eventual consistency strategy
* Live Projections - evaluating a projected view from the raw event data on demand

# Starting a stream.
```
// By key.
var id = "Fourth";
session.Events.StartStream<Quest>(id, joined, departed);
session.SaveChanges();

// By uuid
var id = Guid.NewGuid();
session.Events.StartStream(id, joined, departed);
session.SaveChanges();
```

# Appending to a stream
```
session.Events.Append(id, joined, departed);
session.SaveChanges();

// Events are appended into the stream only if the maximum event id for the stream
// would be 3 after the append operation.
session.Events.Append(id, 3, joined, departed);
session.SaveChanges();
```

# Fetch by Stream
```
// Fetch *all* of the events for this stream
var events1 = await session.Events.FetchStreamAsync(streamId);

// Fetch the events for this stream up to and including version 5
var events2 = await session.Events.FetchStreamAsync(streamId, 5);

// Fetch the events for this stream at this time yesterday
var events3 = await session.Events
  .FetchStreamAsync(streamId, timestamp: DateTime.UtcNow.AddDays(-1));
```

# Fetch Stream State
```
  var state = await theSession.Events.FetchStreamStateAsync(theStreamId).ConfigureAwait(false);
  state.Id.ShouldBe(theStreamId);
  state.Version.ShouldBe(2);
  state.AggregateType.ShouldBe(typeof(Quest));
  state.LastTimestamp.ShouldNotBe(DateTime.MinValue);
  state.Created.ShouldNotBe(DateTime.MinValue);
```

# Batch
```
  // State
  var batch = theSession.CreateBatchQuery();
  var stateTask = batch.Events.FetchStreamState(theStreamId);

  await batch.Execute().ConfigureAwait(false);
  var state = await stateTask.ConfigureAwait(false);

  state.Id.ShouldBe(theStreamId);
  state.Version.ShouldBe(2);
  state.AggregateType.ShouldBe(typeof(Quest));
  state.LastTimestamp.ShouldNotBe(DateTime.MinValue);

  // Events
  var batch = theSession.CreateBatchQuery();
  var eventsTask = batch.Events.FetchStream(theStreamId);

  await batch.Execute().ConfigureAwait(false);
  var events = await eventsTask.ConfigureAwait(false);
  events.Count.ShouldBe(2);
```

# Fetch Single Event
```
  // If you know what the event type is already
  var event1 = await session.Events.LoadAsync<MembersJoined>(eventId)
    .ConfigureAwait(false);

  // If you do not know what the event type is
  var event2 = await session.Events.LoadAsync(eventId)
    .ConfigureAwait(false);
```

Reference:
https://github.com/altairsix/eventsource

Event sourcing is the idea that rather than storing the current state of a domain model into the database, you can instead store the sequence of events (or facts) and then rebuild the domain model from those facts.

git is a great analogy. each commit becomes an event and when you clone or pull the repo, git uses that sequence of commits (events) to rebuild the project file structure (the domain model).

__Event__ (RxwebTask)
observable

Events represent domain events and should be expressed in the past tense such as CustomerMoved, OrderShipped, or EmailAddressChanged. These are irrefutable facts that have completed in the past.

Try to avoid sticking derived values into the events as (a) events are long lived and bugs in the events will cause you great grief and (b) business rules change over time, sometimes retroactively.

__Aggregate__
observable -> aggregate
observable -> transform -> resource -> aggregate

http-post -> event<adt_a08> -> store<event<adt_a08>> -> aggregate<adt_a08> -> store<aggregate<hl7>>
  -> transform<adt_a08> -> event<encounter> -> store<event<encounter>> -> aggregate<encounter> -> store<aggregate<encounter>>
                        -> event<observation> -> store<event<observation>> -> aggregate<observation> -> store<aggregate<observation>>
                        -> event<patient> -> store<event<patient>> -> aggregate<patient> -> store<aggregate<patient>>

http-post -> observable -> store -> aggregate -> store -> observable -> transform -> observable -> store -> aggregate -> store

The Aggregate (often called Aggregate Root) represents the domain modeled by the bounded context and represents the current state of our domain model.

__Repository__

Provides the data access layer to store and retrieve events into a persistent store.

__Store__

Represents the underlying data storage mechanism. eventsource only supports dynamodb out of the box, but there's no reason future versions could not support other database technologies like MySQL, Postgres or Mongodb.

__Serializer__

Specifies how events should be serialized. eventsource currently uses simple JSON serialization although I have some thoughts to support avro in the future.

__CommandHandler__ (RxwebMiddleware)
http-post -> observable 

CommandHandlers are responsible for accepting (or rejecting) commands and emitting events. By convention, the struct that implements Aggregate should also implement CommandHandler.

__Command__ (RxwebTask)
observable

An active verb that represents the mutation one wishes to perform on the aggregate.

__Dispatcher__
on_next

Responsible for retrieving or instantiates the aggregate, executes the command, and saving the the resulting event(s) back to the repository.

