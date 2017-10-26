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
DROP TABLE IF EXISTS {databaseSchema}.mt_streams CASCADE;
CREATE TABLE {databaseSchema}.mt_streams (
	id					uuid CONSTRAINT pk_mt_streams PRIMARY KEY,
	type				varchar(100) NULL,
	version				integer NOT NULL,
	timestamp			timestamptz default (now()) NOT NULL,
	snapshot			jsonb,
	snapshot_version	integer	
);

DROP SEQUENCE IF EXISTS {databaseSchema}.mt_events_sequence;
CREATE SEQUENCE {databaseSchema}.mt_events_sequence;

DROP TABLE IF EXISTS {databaseSchema}.mt_events;
CREATE TABLE {databaseSchema}.mt_events (
	seq_id		bigint CONSTRAINT pk_mt_events PRIMARY KEY,
	id		uuid NOT NULL,
	stream_id	uuid REFERENCES {databaseSchema}.mt_streams ON DELETE CASCADE,
	version		integer NOT NULL,
	data		jsonb NOT NULL,
	type 		varchar(100) NOT NULL,
	timestamp	timestamptz default (now()) NOT NULL,
	CONSTRAINT pk_mt_events_stream_and_version UNIQUE(stream_id, version),
	CONSTRAINT pk_mt_events_id_unique UNIQUE(id)
);

ALTER SEQUENCE {databaseSchema}.mt_events_sequence OWNED BY {databaseSchema}.mt_events.seq_id;


CREATE OR REPLACE FUNCTION {databaseSchema}.mt_append_event(stream uuid, stream_type varchar, event_ids uuid[], event_types varchar[], bodies jsonb[]) RETURNS int[] AS $$
DECLARE
	event_version int;
	event_type varchar;
	event_id uuid;
	body jsonb;
	index int;
	seq int;
	return_value int[];
BEGIN
	select version into event_version from {databaseSchema}.mt_streams where id = stream;
	if event_version IS NULL then
		event_version = 0;
		insert into {databaseSchema}.mt_streams (id, type, version, timestamp) values (stream, stream_type, 0, now());
	end if;


	index := 1;
	return_value := ARRAY[event_version + array_length(event_ids, 1)];

	foreach event_id in ARRAY event_ids
	loop
	    seq := nextval('{databaseSchema}.mt_events_sequence');
		return_value := array_append(return_value, seq);

	    event_version := event_version + 1;
		event_type = event_types[index];
		body = bodies[index];

		insert into {databaseSchema}.mt_events 
			(seq_id, id, stream_id, version, data, type) 
		values 
			(seq, event_id, stream, event_version, body, event_type);

		
		index := index + 1;
	end loop;

	update {databaseSchema}.mt_streams set version = event_version, timestamp = now() where id = stream;

	return return_value;
END
$$ LANGUAGE plpgsql;




DROP TABLE IF EXISTS {databaseSchema}.mt_event_progression CASCADE;
CREATE TABLE {databaseSchema}.mt_event_progression (
	name				varchar CONSTRAINT pk_mt_event_progression PRIMARY KEY,
	last_seq_id			bigint NULL
);



CREATE OR REPLACE FUNCTION {databaseSchema}.mt_mark_event_progression(name varchar, last_encountered bigint) RETURNS VOID LANGUAGE plpgsql AS $function$
BEGIN
INSERT INTO {databaseSchema}.mt_event_progression (name, last_seq_id) VALUES (name, last_encountered)
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