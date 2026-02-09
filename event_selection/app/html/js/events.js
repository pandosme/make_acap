/*
 * events.js - AXIS Camera Event Discovery
 * Queries the device VAPIX SOAP API for available event declarations
 * and parses them into a structured array for use in event selection UI.
 */

function parseSOAPResponse(xmlString) {
	const parser = new DOMParser();
	const xmlDoc = parser.parseFromString(xmlString, "application/xml");

	const parseError = xmlDoc.getElementsByTagName("parsererror");
	if (parseError.length > 0) {
		console.error("Error parsing XML:", parseError[0].textContent);
		return [];
	}

	const result = [];

	function traverseTopics(element, parentNamespace, topicHierarchy) {
		parentNamespace = parentNamespace || null;
		topicHierarchy = topicHierarchy || [];

		for (const child of element.children) {
			const tagParts = child.tagName.split(":");
			let namespace, tagName;

			if (tagParts.length === 2) {
				namespace = tagParts[0];
				tagName = tagParts[1];
			} else if (tagParts.length === 1) {
				namespace = parentNamespace;
				tagName = tagParts[0];
			}

			const currentTopic = {};
			currentTopic[namespace || ""] = tagName;
			const newHierarchy = topicHierarchy.concat([currentTopic]);

			if (child.getAttribute("wstop:topic") === "true") {
				const niceName =
					child.getAttribute("aev:NiceName") ||
					(topicHierarchy.length > 0
						? Object.values(topicHierarchy[topicHierarchy.length - 1])[0] + ":" + tagName
						: tagName);

				const eventObject = { name: niceName };

				newHierarchy.forEach(function(topic, index) {
					eventObject["topic" + index] = topic;
				});

				// Filter out system-internal events that are not useful as triggers
				var dominated = false;

				if (eventObject.topic0) {
					if (eventObject.topic0.tns1 === "RecordingConfig" ||
						eventObject.topic0.tnsaxis === "RecordingConfig" ||
						eventObject.topic0.tnsaxis === "MediaClip") {
						dominated = true;
					}
				}
				if (eventObject.topic1) {
					if (eventObject.topic1.tnsaxis === "SystemMessage" ||
						eventObject.topic1.tnsaxis === "Log" ||
						eventObject.topic1.tnsaxis === "MQTT") {
						dominated = true;
					}
				}

				if (!dominated) {
					result.push(eventObject);
				}
			}

			traverseTopics(child, namespace, newHierarchy);
		}
	}

	const body = xmlDoc.getElementsByTagName("SOAP-ENV:Body")[0];
	if (!body) {
		console.error("No <SOAP-ENV:Body> found in XML.");
		return [];
	}

	const response = body.getElementsByTagName("aev:GetEventInstancesResponse")[0];
	if (!response) {
		console.error("No <aev:GetEventInstancesResponse> found in XML.");
		return [];
	}

	const topicSet = response.getElementsByTagName("wstop:TopicSet")[0];
	if (!topicSet) {
		console.error("No <wstop:TopicSet> found in XML.");
		return [];
	}

	traverseTopics(topicSet);

	// Sort events by name for easy browsing
	result.sort(function(a, b) {
		return a.name.localeCompare(b.name);
	});

	console.log("Discovered " + result.length + " events");
	return result;
}

function listEvents() {
	var soapRequest =
		'<?xml version="1.0" encoding="utf-8"?>' +
		'<soap:Envelope xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"' +
		' xmlns:xsd="http://www.w3.org/2001/XMLSchema"' +
		' xmlns:aev="http://www.axis.com/vapix/ws/event1"' +
		' xmlns:wstop="http://docs.oasis-open.org/wsn/t-1"' +
		' xmlns:soap="http://www.w3.org/2003/05/soap-envelope">' +
		'<soap:Body>' +
		'<aev:GetEventInstances xmlns="http://www.axis.com/vapix/ws/event1">' +
		'</aev:GetEventInstances>' +
		'</soap:Body>' +
		'</soap:Envelope>';

	return fetch('/vapix/services', {
		method: 'POST',
		headers: {
			'Content-Type': 'application/soap+xml; charset=utf-8'
		},
		body: soapRequest
	})
	.then(function(response) {
		if (!response.ok) {
			throw new Error('HTTP error! status: ' + response.status);
		}
		return response.text();
	})
	.then(function(xmlString) {
		return parseSOAPResponse(xmlString);
	})
	.catch(function(error) {
		console.error('Error fetching events:', error);
		return [];
	});
}
